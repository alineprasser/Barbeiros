/*
Aline Bravin Prasser
20181bsi0210
*/

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
  int first;
  int last;
  int countPosition;

  pCustomer data;
} Queue, *pQueue;

typedef struct attendance {
  int qtBarber;
  int qtSpot;
  int qtCustomerTotal;
  int qtHaircutGoal;
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

  bool asleep;
  bool finalizingService;

  pAttendance attendance;
} tBarber, *pBarber;

void showResults(int qtBarber, pBarber barberList[]);
bool roomHasSlots(int qtSpot, int qtWaiting, int max);
bool couchHasSlots(int qtCouch, int max);
bool queueHasSlots(pQueue queue);
bool reachedGoal(pBarber barber);
void createQueue(pQueue queue, int ammount);
void insertQueue(pQueue queue, pCustomer customer);
void createBarberQueue(pBarber barberList[], pAttendance attendance);
Customer serveCustomer(pQueue queue);
void allocateCouch(pQueue couchQueue, pQueue waitingQueue);
void awaitPayment(pQueue couchQueue, pQueue paymentQueue);
void releaseCustomer(pQueue queue);
void *customerThread(void *param);
void *barberThread(void *param);
bool checkService(pBarber barberList[], int qtBarber, pAttendance attendance);
void initializeWork(pAttendance attendance, int qtBarber, int qtSpot,
                    int qtCustomerTotal, int qtHaircutGoal, pQueue waitingQueue,
                    pQueue couchQueue, pQueue paymentQueue,
                    pthread_mutex_t m_queue, pthread_mutex_t m_barber,
                    pthread_mutex_t m_cashier, sem_t barber_semaphore);

void showResults(int qtBarber, pBarber barberList[]) {
  // exibe os resultados finais no console
  for (int i = 0; i < qtBarber; i++) {
    printf("Barbeiro %d atendeu %d clientes\n", barberList[i]->id + 1,
           barberList[i]->qtHaircutsDone);
  }
}

bool roomHasSlots(int qtSpot, int qtWaiting, int max) {
  // verifica se há espaços no salão
  return qtSpot + qtWaiting < max ? true : false;
}

bool couchHasSlots(int qtCouch, int max) {
  // verifica se há espaços no sofá
  return qtCouch < max ? true : false;
}

bool queueHasSlots(pQueue queue) {
  // verifica se há espaços na fila
  return queue->countPosition > 0 ? true : false;
}

bool reachedGoal(pBarber barber) {
  // verifica se o barbeiro atingiu a meta
  return barber->qtHaircutsDone >= barber->attendance->qtHaircutGoal ? true
                                                                     : false;
}

void createQueue(pQueue queue, int ammount) {
  // inicializa a fila com a quantidade passada pelo parâmtro
  queue->max = ammount;
  queue->data = (pCustomer)malloc(ammount * sizeof(Customer));
  queue->first = 0;
  queue->last = -1;
  queue->countPosition = 0;
}

void insertQueue(pQueue queue, pCustomer customer) {
  // insere o cliente na fila
  queue->last = queue->last == queue->max - 1 ? -1 : queue->last;
  queue->last++;
  queue->data[queue->last] = *(pCustomer)customer;
  queue->countPosition++;
}

void createBarberQueue(pBarber barberList[], pAttendance attendance) {
  // inicializa os barbeiros na fila do atendimento
  int id = 0;
  while (id < attendance->qtBarber) {
    pBarber barber = (pBarber)malloc(sizeof(tBarber));
    barberList[id] = barber;
    barber->id = id;
    barber->attendance = attendance;
    barber->qtHaircutsDone = 0;
    barber->reachedHaircutGoal = 0;
    barber->asleep = true;
    barber->finalizingService = false;
    id++;
  }
}

Customer serveCustomer(pQueue queue) {
  // atendimento ao cliente
  Customer customer = queue->data[queue->first++];
  queue->first = queue->first == queue->max ? 0 : queue->first;
  queue->countPosition--;
  return customer;
}

void allocateCouch(pQueue couchQueue, pQueue waitingQueue) {
  // aloca o cliente no sofá
  Customer altCustomer = waitingQueue->data[waitingQueue->first++];
  pCustomer customer = (pCustomer)malloc(sizeof(Customer));
  customer->id = altCustomer.id;
  insertQueue(couchQueue, customer);
  waitingQueue->first =
      waitingQueue->first == waitingQueue->max ? 0 : waitingQueue->first;
  waitingQueue->countPosition--;
}

void awaitPayment(pQueue couchQueue, pQueue paymentQueue) {
  // adiciona o cliente na fila de pagamento
  Customer altCustomer = paymentQueue->data[paymentQueue->first++];
  pCustomer customer = (pCustomer)malloc(sizeof(Customer));
  customer->id = altCustomer.id;
  insertQueue(couchQueue, customer);
  paymentQueue->first =
      paymentQueue->first == paymentQueue->max ? 0 : paymentQueue->first;
  paymentQueue->countPosition--;
}

void releaseCustomer(pQueue queue) {
  // libera o cliente
  Customer aux = queue->data[queue->first++];
  queue->first = queue->first == queue->max ? 0 : queue->first;
  queue->countPosition--;
}

void *customerThread(void *param) {
  // declaração das operações com cliente
  pBarber *barberList = (pBarber *)param;
  pAttendance attendance = barberList[0]->attendance;

  pQueue couchQueue = attendance->couchQueue;
  int qtWaitingCouch = couchQueue->countPosition;

  pQueue waitingQueue = attendance->waitingQueue;
  int qtWaitingQueue = waitingQueue->countPosition;

  while (1) {
    pthread_mutex_lock(attendance->m_queue);
    pthread_mutex_lock(attendance->m_barber);

    if (roomHasSlots(qtWaitingCouch, qtWaitingQueue,
                     attendance->qtCustomerTotal)) {
      pthread_mutex_unlock(attendance->m_barber);

      pCustomer customer = (pCustomer)malloc(sizeof(Customer));
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
  // declaração das operações com barbeiro
  pBarber barber = (pBarber)param;
  pQueue couchQueue = barber->attendance->couchQueue;
  pQueue waitingQueue = barber->attendance->waitingQueue;

  while (true) {
    pthread_mutex_lock(barber->attendance->m_queue);
    if (queueHasSlots(couchQueue) && !barber->attendance->isDone) {
      Customer customer = serveCustomer(couchQueue);

      if (queueHasSlots(waitingQueue)) {
        allocateCouch(couchQueue, waitingQueue);
      }

      pthread_mutex_lock(barber->attendance->m_barber);

      pthread_mutex_unlock(barber->attendance->m_queue);

      barber->asleep = false;
      barber->qtHaircutsDone++;

      if (reachedGoal(barber)) {
        barber->reachedHaircutGoal = 1;
      }

      awaitPayment(barber->attendance->couchQueue,
                   barber->attendance->paymentQueue);

      pthread_mutex_unlock(barber->attendance->m_barber);

      barber->asleep = true;
    } else {
      pthread_mutex_unlock(barber->attendance->m_barber);
      pthread_mutex_unlock(barber->attendance->m_queue);
    }

    pthread_mutex_lock(barber->attendance->m_cashier);
    pthread_mutex_lock(barber->attendance->m_barber);

    if (queueHasSlots(barber->attendance->paymentQueue)) {
      barber->asleep = false;
      barber->finalizingService = true;
      releaseCustomer(barber->attendance->paymentQueue);
    }

    pthread_mutex_unlock(barber->attendance->m_barber);
    pthread_mutex_unlock(barber->attendance->m_cashier);

    barber->asleep = true;
    barber->finalizingService = false;
    sem_wait(barber->attendance->barber_semaphore);
  }
  return 0;
}

bool checkService(pBarber barberList[], int qtBarber, pAttendance attendance) {
  // verifica se há algum barbeiro que não atingiu a meta ainda
  if (qtBarber > 0) {
    for (int i = 0; i < qtBarber; i++) {
      if (barberList[i]->reachedHaircutGoal == 0) {
        return true;
      }
    }
    attendance->isDone = true;
    return false;
  }
}

void initializeWork(pAttendance attendance, int qtBarber, int qtSpot,
                    int qtCustomerTotal, int qtHaircutGoal, pQueue waitingQueue,
                    pQueue couchQueue, pQueue paymentQueue,
                    pthread_mutex_t m_queue, pthread_mutex_t m_barber,
                    pthread_mutex_t m_cashier, sem_t barber_semaphore) {
  // inicialização dos valores padrões do atendimento
  attendance->qtBarber = qtBarber;
  attendance->qtSpot = qtSpot;
  attendance->qtHaircutGoal = qtHaircutGoal;
  attendance->qtCustomerTotal = qtCustomerTotal;
  attendance->qtHaircutsDoneTotal = 0;
  attendance->isDone = false;

  attendance->waitingQueue = waitingQueue;
  attendance->couchQueue = couchQueue;
  attendance->paymentQueue = paymentQueue;

  attendance->m_queue = &mutex_queue;
  attendance->m_barber = &mutex_barber;
  attendance->m_cashier = &mutex_cashier;
  attendance->barber_semaphore = &attendance_count;
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

  // declaração dos mutexes e semaforo
  pthread_mutex_t m_queue;
  pthread_mutex_t m_barber;
  pthread_mutex_t m_cashier;
  sem_t barber_semaphore;

  pAttendance attendance = (pAttendance)malloc(sizeof(tAttendance));

  pQueue couchQueue = (pQueue)malloc(sizeof(Queue));
  pQueue waitingQueue = (pQueue)malloc(sizeof(Queue));
  pQueue paymentQueue = (pQueue)malloc(sizeof(Queue));

  pBarber barberList[qtSpot];

  // inicializaçao dos valores padrões

  initializeWork(attendance, qtBarber, qtSpot, qtCustomerTotal, qtHaircutGoal,
                 waitingQueue, couchQueue, paymentQueue, m_queue, m_barber,
                 m_cashier, barber_semaphore);

  // criação das filas
  createBarberQueue(barberList, attendance);
  createQueue(couchQueue, qtSpot);
  createQueue(waitingQueue, qtWaiting);
  createQueue(paymentQueue, qtSpot);

  attendance->couchQueue = couchQueue;
  attendance->waitingQueue = waitingQueue;
  attendance->paymentQueue = paymentQueue;

  sem_init(attendance->barber_semaphore, 0, 0);

  // inicialização das threads
  pthread_t *threadsArray = malloc(qtBarber * sizeof(pthread_t));

  int result = 0;

  for (int i = 0; i < qtBarber; i++) {
    result = pthread_create(&threadsArray[i], NULL, barberThread,
                            (void *)barberList[i]);
    if (i == qtBarber - 1) {
      result = pthread_create(&threadsArray[i], NULL, customerThread,
                              (void *)barberList);
    }

    if (result != 0) {
      printf("Falha ao tentar criar a thread!");
      exit(1);
    }
  }

  bool repeatExecution = true;
  // verificação dos atendimentos
  while (repeatExecution) {
    repeatExecution = checkService(barberList, qtBarber, attendance);
  }

  showResults(qtBarber, barberList);

  free(waitingQueue);
  free(paymentQueue);
  free(couchQueue);
  free(attendance);
  free(threadsArray);
  sem_destroy(&attendance_count);
}