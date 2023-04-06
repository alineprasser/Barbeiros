#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>

int finished(int *var, pthread_mutex_t *mutex);
void* barber(void* arg);
void* customer(void* arg);

typedef struct {
  sem_t *semCustomer, *semBarber, *semChair;
} Customer;

typedef struct {
  int qtBarbeiros, qtLugares, qtTotalClientes, *workingBarbers;
  sem_t *semChair, *semBarber;
  pthread_mutex_t *mutexOperation;
} Barber;

int main2(int argc, char** argv) {
  if (argc < 4) {
    printf("Quantidade insuficiente de argumentos. \n");
    printf("Deverão ser informados obrigatoriamente: \n");
    printf("1.Quantidade de Barbeiros;\n2.Quantidade de lugares no sofá de espera;\n3.Quantidade de clientes permitidos na barbearia;\n4.Quantidade mínima de atendimentos por barber. \n");
    exit(0);
  }
  
  int qtBarbeiros = atoi(argv[1]);
  int qtLugares = atoi(argv[2]);
  int qtTotalClientes = atoi(argv[3]);
	int qtMetaAtendimento = atoi(argv[4]); 
  
  int workingBarbers = qtBarbeiros;

  sem_t semCustomer, semBarber, semChair;
  sem_init(&semBarber, 0, 0);
  sem_init(&semCustomer, 0, qtLugares);
  sem_init(&semChair, 0, qtBarbeiros);

  pthread_mutex_t mutexOperation;
  pthread_mutex_init(&mutexOperation, NULL);
  
  Customer *customerArg = &(Customer) { &semCustomer, &semBarber, &semChair };
  Barber *barberArg = &(Barber) {
    qtBarbeiros, qtLugares, qtTotalClientes, &workingBarbers,
    &semChair, &semBarber,
    &mutexOperation
  };

  pthread_t barbers_threads[qtBarbeiros];
  for(int i = 0; i < qtBarbeiros; i++) {
    pthread_create(&barbers_threads[i], NULL, barber, (void*) barberArg);
  }

  while (!finished(&workingBarbers, &mutexOperation)) {
    pthread_t client_thread;
    pthread_create(&client_thread, NULL, customer, (void*) customerArg);
  }

  for(int i = 0; i < qtBarbeiros; i++) {
    sem_post(&semBarber); // Acorda os barbeiros dormindo
  }

  for(int i = 0; i < qtBarbeiros; i++) {
    void* value;
    pthread_join(barbers_threads[i], &value);
    printf("barbeiro %d atendeu %d clientes\n", i, (int) value);
  }

  pthread_mutex_destroy(&mutexOperation);
  
  sem_destroy(&semBarber);
  sem_destroy(&semCustomer);
  sem_destroy(&semChair);

  return 0;
}

void* barber(void* arg) {
  Barber *arg_content = (Barber*) arg;
  int haircuts = 0;

  while (!finished(arg_content->workingBarbers, arg_content->mutexOperation)) {
    sem_wait(arg_content->semBarber); // Dorme na cadeira
    sem_post(arg_content->semChair); // Corta o cabelo

    if (++haircuts == arg_content->qtTotalClientes) {
      pthread_mutex_lock(arg_content->mutexOperation);
      (*arg_content->workingBarbers)--;
      pthread_mutex_unlock(arg_content->mutexOperation);
    }
  }

  return (void*) haircuts;
}

void* customer(void* arg) {
  Customer *arg_content = (Customer*) arg;

  // Tenta entrar na barbearia
  if (sem_trywait(arg_content->semCustomer) == 0) {
    sem_wait(arg_content->semChair);  // Espera para cortar o cabelo
    sem_post(arg_content->semCustomer); // Libera uma cadeira de espera
    sem_post(arg_content->semBarber); // Acorda um barbeiro
  }

  return NULL;
}

int finished(int *var, pthread_mutex_t *mutex) {
  int value;
  
  pthread_mutex_lock(mutex);
  value = *var;
  pthread_mutex_unlock(mutex);

  return value == 0;
}
