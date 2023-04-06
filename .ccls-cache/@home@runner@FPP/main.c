#include <pthread.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>

pthread_mutex_t mutex_queue;
pthread_mutex_t mutex_barber;
pthread_mutex_t mutex_cashier;
sem_t attendance_count;

typedef enum { false, true } bool;

typedef struct customer {
  int id;
} Customer, *pCustomer;

typedef struct queue {
  int max;
  pCustomer data;
  int first;
  int last;
  int qtPosition;
} Queue, *pQueue;

typedef struct attendance {
  int qtBarber;
  int qtSpot;
  int qtHaircutGoal;
  int qtCustomerTotal;
  int qtHaircutsDoneTotal;
  bool isDone;
  pQueue waitingQueue;
  pQueue couchQueue;
  pQueue paymentQueue;
  pthread_mutex_t *m_queue;
  pthread_mutex_t *m_barber;
  pthread_mutex_t *m_cashier;
  sem_t *barber_semaphore;
} tAttendance, *pAttendance;

typedef struct barber {
  int id;
  int qtHaircutsDone;
  int reachedHaircutGoal;
  bool awaiting;
  bool finalizingService;
  pAttendance attendance;
} tBarber, *pBarber;

void showResults(int qtBarber, pBarber barberList[]) {
  for (int i = 0; i < qtBarber; i++) {
    printf("Barbeiro %d atendeu %d\n", barberList[i]->id + 1,
           barberList[i]->qtHaircutsDone);
  }
}

int roomHasSlots(int qtSpot, int qtWaiting, int max) {
  return qtSpot + qtWaiting < max ? 1 : 0;
}

int couchHasSlots(int qtCouch, int max) { return qtCouch < max ? 1 : 0; }

int reachedGoal(pBarber barber) {
  return barber->qtHaircutsDone >= barber->attendance->qtHaircutGoal ? 1 : 0;
}

void createQueue(pQueue queue, int ammount) {
  queue->max = ammount;
  queue->data = (pCustomer)malloc(ammount * 2 * sizeof(Customer));
  queue->first = 0;
  queue->last = -1;
  queue->qtPosition = 0;
}

void insertQueue(pQueue queue, pCustomer customer) {
  queue->last = queue->last == queue->max - 1 ? -1 : queue->last;
  queue->last++;
  queue->data[queue->last] = *(pCustomer)customer;
  queue->qtPosition++;
}

int queueHasSlots(pQueue queue) { return (queue->qtPosition > 0); }

void createBarberQueue(pBarber barberList[], pAttendance attendance) {
  int totalBarber = attendance->qtBarber;
  int id = 0;
  while (id < totalBarber) {
    pBarber barber = (pBarber)malloc(1 * sizeof(tBarber));
    barber->id = id;
    barber->qtHaircutsDone = 0;
    barber->reachedHaircutGoal = 0;
    barber->awaiting = true;
    barber->finalizingService = false;
    barber->attendance = attendance;
    barberList[id] = barber;
    id++;
  }
}

Customer serveCustomer(pQueue queue) {
  Customer customer = queue->data[queue->first++];
  queue->first = queue->first == queue->max ? 0 : queue->first;
  queue->qtPosition--;
  return customer;
}

void allocateCouch(pQueue couchQueue, pQueue waitingQueue) {
  Customer altCustomer = waitingQueue->data[waitingQueue->first++];
  pCustomer customer = (pCustomer)malloc(1 * sizeof(Customer));
  customer->id = altCustomer.id;
  insertQueue(couchQueue, customer);
  waitingQueue->first = waitingQueue->first == waitingQueue->max ? 0 : waitingQueue->first;
  waitingQueue->qtPosition--;
}

void awaitRelease(pQueue couchQueue, pQueue paymentQueue) {
  Customer altCustomer = paymentQueue->data[paymentQueue->first++];
  pCustomer customer = (pCustomer)malloc(1 * sizeof(Customer));
  customer->id = altCustomer.id;
  insertQueue(couchQueue, customer);
  paymentQueue->first = paymentQueue->first == paymentQueue->max ? 0 : paymentQueue->first;
  paymentQueue->qtPosition--;
}

void releaseCustomer(pQueue queue) {
  Customer aux = queue->data[queue->first++];
  queue->first = queue->first == queue->max ? 0 : queue->first;
  queue->qtPosition--;
}

void *customerThread(void *param) {
  pBarber *barberList = (pBarber *)param;
  pAttendance attendance = barberList[0]->attendance;
  pQueue couchQueue = attendance->couchQueue;
  pQueue waitingQueue = attendance->waitingQueue;
  int qtWaitingQueue = 0;
  int qtWaitingCouch = 0;
  int qtCustomerTotal = attendance->qtCustomerTotal;

  while (1) {
    qtWaitingCouch = couchQueue->qtPosition;
    qtWaitingQueue = waitingQueue->qtPosition;

    pthread_mutex_lock(attendance->m_queue);
    pthread_mutex_lock(attendance->m_barber);

    if (roomHasSlots(qtWaitingCouch, qtWaitingQueue, qtCustomerTotal)) {
      pthread_mutex_unlock(attendance->m_barber);
      pCustomer customer = (pCustomer)malloc(1 * sizeof(Customer));
      customer->id = attendance->qtHaircutsDoneTotal + 1;

      attendance->qtHaircutsDoneTotal++;

      if (couchHasSlots(qtWaitingCouch, couchQueue->max)) {
        insertQueue(couchQueue, customer);
      } else {
        insertQueue(waitingQueue, customer);
      }

      pthread_mutex_unlock(attendance->m_queue);

    } else {
      pthread_mutex_unlock(attendance->m_barber);
      pthread_mutex_unlock(attendance->m_queue);
    }
    sem_post(attendance->barber_semaphore);
  }
}

void *barberThread(void *param) {
  pBarber barber = (pBarber)param;
  pQueue couchQueue = barber->attendance->couchQueue;
  pQueue waitingQueue = barber->attendance->waitingQueue;

  while (1) {
    pthread_mutex_lock(barber->attendance->m_queue);
    if (queueHasSlots(couchQueue) && !barber->attendance->isDone) {
      Customer customer = serveCustomer(couchQueue);
      if (queueHasSlots(waitingQueue)) {
        allocateCouch(couchQueue, waitingQueue);
      }
      pthread_mutex_unlock(barber->attendance->m_queue);
      pthread_mutex_lock(barber->attendance->m_barber);
      barber->awaiting = false;
      barber->qtHaircutsDone++;
      if (reachedGoal(barber)) {
        barber->reachedHaircutGoal = 1;
      }
      awaitRelease(barber->attendance->couchQueue,
                   barber->attendance->paymentQueue);
      pthread_mutex_unlock(barber->attendance->m_barber);
      barber->awaiting = true;

    } else {
      pthread_mutex_unlock(barber->attendance->m_queue);
      pthread_mutex_unlock(barber->attendance->m_barber);
    }

    pthread_mutex_lock(barber->attendance->m_cashier);
    pthread_mutex_lock(barber->attendance->m_barber);

    if (queueHasSlots(barber->attendance->paymentQueue)) {
      barber->awaiting = false;
      barber->finalizingService = true;
      releaseCustomer(barber->attendance->paymentQueue);
    }
    pthread_mutex_unlock(barber->attendance->m_barber);
    pthread_mutex_unlock(barber->attendance->m_cashier);
    barber->awaiting = true;
    barber->finalizingService = false;
    sem_wait(barber->attendance->barber_semaphore);
  }

  return 0;
}

int doService(pBarber barberList[], int qtBarber, pAttendance attendance) {
  if (qtBarber > 0) {
    for (int i = 0; i < qtBarber; i++) {
      if (barberList[i]->reachedHaircutGoal == 0) {
        return 1;
      }
    }
    attendance->isDone = true;
    return 0;
  }
}

void initializeWork(pAttendance attendance, int qtBarber, int qtSpot,
                    int qtHaircutGoal, int qtCustomerTotal,
                    pthread_mutex_t m_queue, pthread_mutex_t m_barber,
                    pthread_mutex_t m_cashier, sem_t barber_semaphore,
                    pQueue couchQueue, pQueue waitingQueue,
                    pQueue paymentQueue) {
  attendance->m_queue = &mutex_queue;
  attendance->m_barber = &mutex_barber;
  attendance->m_cashier = &mutex_cashier;
  attendance->barber_semaphore = &attendance_count;

  attendance->qtHaircutGoal = qtHaircutGoal;
  attendance->qtBarber = qtBarber;
  attendance->qtSpot = qtSpot;
  attendance->qtCustomerTotal = qtCustomerTotal;

  attendance->couchQueue = couchQueue;
  attendance->waitingQueue = waitingQueue;
  attendance->paymentQueue = paymentQueue;

  attendance->isDone = false;
  attendance->qtHaircutsDoneTotal = 0;
}

int main(int argc, char **argv) {
  if (argc < 5) {
    printf("Quantidade insuficiente de argumentos. \n");
    printf("Deverão ser informados obrigatoriamente: \n");
    printf("1.Quantidade de Barbeiros;\n2.Quantidade de lugares no sofá de "
           "espera;\n3.Quantidade de clientes permitidos na "
           "barbearia;\n4.Quantidade mínima de atendimentos por barbeiro. \n");
    exit(0);
  }

  int qtBarber = atoi(argv[1]);        // quantidade de barbeiros
  int qtSpot = atoi(argv[2]);          // quantidade de lugares
  int qtCustomerTotal = atoi(argv[3]); // quantidade de clientes
  int qtHaircutGoal = atoi(argv[4]);   // quantidade mínima de atendimentos

  int qtWaiting = qtCustomerTotal - qtSpot >= 0 ? qtCustomerTotal - qtSpot
                                                : 0; // quantidade em espera

  // declaração dos mutex e semaforo
  pthread_mutex_t m_queue;
  pthread_mutex_t m_barber;
  pthread_mutex_t m_cashier;
  sem_t barber_semaphore;

  pAttendance attendance = (pAttendance)malloc(2 * sizeof(tAttendance));

  pQueue waitingQueue = (pQueue)malloc(2 * sizeof(Queue));
  pQueue couchQueue = (pQueue)malloc(2 * sizeof(Queue));
  pQueue paymentQueue = (pQueue)malloc(2 * sizeof(Queue));
  pBarber barberList[qtSpot];

  initializeWork(attendance, qtBarber, qtSpot, qtHaircutGoal, qtCustomerTotal,
                 m_queue, m_barber, m_cashier, barber_semaphore, couchQueue,
                 waitingQueue, paymentQueue);

  createBarberQueue(barberList, attendance);
  createQueue(couchQueue, qtSpot);
  createQueue(waitingQueue, qtWaiting);
  createQueue(paymentQueue, qtSpot);

  attendance->waitingQueue = waitingQueue;
  attendance->couchQueue = couchQueue;
  attendance->paymentQueue = paymentQueue;

  sem_init(attendance->barber_semaphore, 0, 0);

  pthread_t *vetor_threads = malloc(qtBarber * sizeof(pthread_t));

  int result = 0;

  for (long cont = 0; cont < qtBarber; cont++) {
    result = pthread_create(&vetor_threads[cont], NULL, barberThread,
                            (void *)barberList[cont]);
    if (cont == qtBarber - 1) {
      result = pthread_create(&vetor_threads[cont], NULL, customerThread,
                              (void *)barberList);
    }

    if (result != 0) {
      printf("Falha ao tentar criar a thread!");
      exit(1);
    }
  }

  int repeatExecution = 1;

  while (repeatExecution == 1) {
    repeatExecution = doService(barberList, qtBarber, attendance);
  }

  showResults(qtBarber, barberList);

  free(waitingQueue);
  free(paymentQueue);
  free(couchQueue);
  free(attendance);
  free(vetor_threads);
  sem_destroy(&attendance_count);
}