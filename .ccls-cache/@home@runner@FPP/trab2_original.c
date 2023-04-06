#include <pthread.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct cliente {

  int ident;

} T_Cliente, *P_Cliente;

typedef struct clientFila {
  int limite;
  P_Cliente dado;
  int primeiro;
  int ultimo;
  int quantItens;
} T_clientFila, *P_clientFila;

typedef struct operacoes {

  int quantMinimaAtendimento;
  int quantBarbeiroCadeira;
  int quantCadeiraCliente;

  P_clientFila clientFila;
  int todosAtenderamMinimo;

  sem_t *semaph_barbeiro;
  pthread_mutex_t *mutex_fila;
  pthread_mutex_t *mutex_barb;

} T_operacoes, *P_operacoes;

typedef struct barbeiro {

  int ident;
  int quantidadeAtendimento;
  int finalizou;
  int dormindo;
  P_operacoes op;

} T_Barbeiro, *P_Barbeiro;

pthread_mutex_t mutex_one;
pthread_mutex_t mutex_two;
sem_t semaphore_cont;

void f_montaOperacoes(P_operacoes op, int quantMinimaAtendimento,
                      int quantBarbeiroCadeira, int quantCadeiraCliente,
                      pthread_mutex_t mutex_fila, pthread_mutex_t mutex_barb,
                      sem_t semaph_barbeiro, P_clientFila fila) {

  op->quantMinimaAtendimento = quantMinimaAtendimento;
  op->quantBarbeiroCadeira = quantBarbeiroCadeira;
  op->quantCadeiraCliente = quantCadeiraCliente;
  op->clientFila = fila;
  op->todosAtenderamMinimo = 0;

  op->mutex_fila = &mutex_one;
  op->mutex_barb = &mutex_two;
  op->semaph_barbeiro = &semaphore_cont;
}

// ################################################
// INICIO - Manipulacao das Cadeiras
// ################################################

// @ Iniciar cadeiras;
void f_iniciaFila(P_clientFila fila, int limite) {

  fila->limite = limite;
  fila->dado = (P_Cliente)malloc(limite * sizeof(T_Cliente));
  fila->primeiro = 0;
  fila->ultimo = -1;
  fila->quantItens = 0;
}

// @ Add pessoa nas cadeiras;
void f_inserirFila(P_clientFila fila, P_Cliente cliente) {

  if (fila->ultimo == fila->limite - 1) {

    fila->ultimo = -1;
  }

  fila->ultimo++;
  fila->dado[fila->ultimo] = *(P_Cliente)cliente;
  fila->quantItens++;
}

// @ Verifica se esta vazia ou cheia
int f_estaVaziaFila(P_clientFila fila) { return (fila->quantItens == 0); }

int f_estaCheiaFila(P_clientFila fila) {

  return (fila->quantItens == fila->limite);
}

// @ Remover da Cadeira
void f_removerClienteCadeira(P_clientFila fila) {

  T_Cliente aux = fila->dado[fila->primeiro++];

  if (fila->primeiro == fila->limite) {
    fila->primeiro == 0;
  }

  fila->quantItens--;
}

// ################################################
// FIM - Manipulacao das Cadeiras
// ################################################

void f_iniciaListaBarbeiro(P_Barbeiro lista[], P_operacoes op) {

  int cont = 0;
  int quantBarbeiro = op->quantBarbeiroCadeira;

  while (cont < quantBarbeiro) {

    P_Barbeiro barbeiro;

    barbeiro = (P_Barbeiro)malloc(1 * sizeof(T_Barbeiro));

    barbeiro->ident = cont;
    barbeiro->quantidadeAtendimento = 0;
    barbeiro->finalizou = 0;
    barbeiro->dormindo = 1;
    barbeiro->op = op;

    lista[cont] = barbeiro;
    cont++;
  }
}

int f_antendeuMinimo(P_Barbeiro barbeiro) {

  // Atendeu o minimo?
  if (barbeiro->quantidadeAtendimento >= barbeiro->op->quantMinimaAtendimento) {
    return 1;
  }
  return 0;
}

int f_barbeiroDisponivel(P_Barbeiro lista[], int tam) {

  for (int i = 0; i < tam; i++) {

    // Se existe algum disponivel
    if (lista[i]->dormindo == 1) {
      return 1;
    }
  }
  return 0;
}

int f_atendeuMinimoTodosBarbeiros(P_Barbeiro listaBarbeiro[],
                                  int quantBarbeiroCadeira, P_operacoes op) {

  if (quantBarbeiroCadeira > 0) {

    for (int i = 0; i < quantBarbeiroCadeira; i++) {

      if (listaBarbeiro[i]->finalizou == 0) {
        return 0;
      }
    }
    op->todosAtenderamMinimo = 1;
    return 1;
  }
}

void *f_threadBarbeiro(void *param) {

  P_Barbeiro barbeiro = (P_Barbeiro)param;
  P_clientFila fila = barbeiro->op->clientFila;

  while (1) {

    pthread_mutex_lock(barbeiro->op->mutex_fila);
    if (!f_estaVaziaFila(fila)) {

      f_removerClienteCadeira(fila);
      pthread_mutex_unlock(barbeiro->op->mutex_fila);

      pthread_mutex_lock(barbeiro->op->mutex_barb);
      barbeiro->dormindo = 0;

      if (!barbeiro->op->todosAtenderamMinimo) {
        barbeiro->quantidadeAtendimento++;
      }

      if (f_antendeuMinimo(barbeiro)) {
        barbeiro->finalizou = 1;
      }
      pthread_mutex_unlock(barbeiro->op->mutex_barb);
      barbeiro->dormindo = 1;

    } else {
      pthread_mutex_unlock(barbeiro->op->mutex_fila);
    }

    sem_wait(barbeiro->op->semaph_barbeiro);
  }

  return 0;
}

void *f_threadClienteOperacao(void *param) {

  P_Barbeiro *lista = (P_Barbeiro *)param;
  P_operacoes op = lista[0]->op;
  P_clientFila fila = op->clientFila;

  while (1) {

    pthread_mutex_lock(op->mutex_fila);
    pthread_mutex_lock(op->mutex_barb);

    if (!(f_barbeiroDisponivel(lista, op->quantBarbeiroCadeira) &&
          !f_estaVaziaFila(fila))) {

      pthread_mutex_unlock(op->mutex_barb);

      // Não esta cheia
      if (!f_estaCheiaFila(fila)) {

        P_Cliente cliente;
        cliente = (P_Cliente)malloc(1 * sizeof(T_Cliente));

        f_inserirFila(fila, cliente);
      }

      pthread_mutex_unlock(op->mutex_fila);

    } else { // Tem gente na fila e esta disponivel

      pthread_mutex_unlock(op->mutex_fila);
      pthread_mutex_unlock(op->mutex_barb);
      sem_post(op->semaph_barbeiro);
    }
  }
}

void f_printListaBarbeiro(P_Barbeiro listaBarbeiro[],
                          int quantBarbeiroCadeira) {

  int quantItens = quantBarbeiroCadeira;

  for (int i = 0; i < quantItens; i++) {

    printf("Barbeiro - %d - Atendeu - %d\n", listaBarbeiro[i]->ident,
           listaBarbeiro[i]->quantidadeAtendimento);
  }
}

int main3(int argc, char **argv) {

  if (argc > 1) {
    int quantBarbeiroCadeira = atoi(argv[1]);
    int quantCadeiraCliente = atoi(argv[2]);
    int quantMinimaAtendimento = atoi(argv[3]);

    pthread_mutex_t mutex_fila;
    pthread_mutex_t mutex_barb;
    sem_t semaph_barbeiro;

    P_operacoes op;
    P_clientFila clientFila;
    P_Barbeiro listaBarbeiro[quantBarbeiroCadeira];

    op = (P_operacoes)malloc(1 * sizeof(T_operacoes));
    clientFila = (P_clientFila)malloc(1 * sizeof(T_clientFila));

    f_montaOperacoes(op, quantMinimaAtendimento, quantBarbeiroCadeira,
                     quantCadeiraCliente, mutex_fila, mutex_barb,
                     semaph_barbeiro, clientFila);

    // MONTAR LISTA BARBEIROS
    f_iniciaListaBarbeiro(listaBarbeiro, op);
    f_iniciaFila(clientFila, quantBarbeiroCadeira);

    op->clientFila = clientFila;

    sem_init(op->semaph_barbeiro, 0, 0);

    int quantThreads = quantBarbeiroCadeira;

    // Cria vetor thread
    pthread_t *vetor_threads;
    vetor_threads = malloc(quantThreads * sizeof(pthread_t));

    int retorno_thread = 0;

    // Cria thread
    for (long cont = 0; cont < quantThreads; cont++) {

      retorno_thread =
          pthread_create(&vetor_threads[cont], NULL, f_threadBarbeiro,
                         (void *)listaBarbeiro[cont]);

      if (cont == quantThreads - 1) {

        retorno_thread =
            pthread_create(&vetor_threads[cont], NULL, f_threadClienteOperacao,
                           (void *)listaBarbeiro);
      }

      if (retorno_thread != 0) {

        printf("Error CREATE");
        exit(-1);
      }
    }

    while (!f_atendeuMinimoTodosBarbeiros(listaBarbeiro, quantBarbeiroCadeira,
                                          op)) {
    }

    f_printListaBarbeiro(listaBarbeiro, quantBarbeiroCadeira);

    free(clientFila);
    free(op);

    free(vetor_threads);
    sem_destroy(op->semaph_barbeiro);
  } else {
    printf("Entrada inválida\n");
  }
}
