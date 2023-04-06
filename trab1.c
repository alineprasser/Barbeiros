#include<pthread.h>
#include<stdio.h>
#include<stdlib.h>
#include<semaphore.h>

typedef struct cliente{
	int ident;
}T_Cliente,*P_Cliente;

typedef struct fila{
	int limite;
	P_Cliente dado;
	int primeiro;
	int ultimo;
	int quantItens;

}T_fila,*P_fila;

typedef struct operacoes
{	
	P_fila filaEmPe;
	P_fila filaSofa;
	P_fila filaPagamento;
	int contBarbeiroCadeira;
	int contSofa;
	int contMinimaAtendimento;
	int contLimiteCliente;	
	int todoAtendidos;
	int totalAtendidoPorTodos;
	pthread_mutex_t *mutex_fila;
	pthread_mutex_t *mutex_barb;
	pthread_mutex_t *mutex_caixa;
	sem_t *semaph_barbeiro; 
	
}T_operacoes,*P_operacoes;

typedef struct barbeiro{

	int ident;
	int quantidadeAtendimento;
	int finalizou;
	int dormindo;
	int recebendoPagamento;
	P_operacoes operacoes;

}T_Barbeiro,*P_Barbeiro;

pthread_mutex_t mutex_1;
pthread_mutex_t mutex_2;
pthread_mutex_t mutex_3;
sem_t semaph_cont;

void iniciaOperacoes(P_operacoes operacoes,
	int contBarbeiroCadeira, int contSofa,int contMinimaAtendimento, int contLimiteCliente,
	pthread_mutex_t mutex_fila, pthread_mutex_t mutex_barb, pthread_mutex_t mutex_caixa,
	sem_t semaph_barbeiro, P_fila filaSofa, P_fila filaEmPe, P_fila filaPagamento){

	operacoes->mutex_fila = &mutex_1;
	operacoes->mutex_barb = &mutex_2;
	operacoes->mutex_caixa = &mutex_3;
	operacoes->semaph_barbeiro = &semaph_cont;
	operacoes->contMinimaAtendimento = contMinimaAtendimento;
	operacoes->contBarbeiroCadeira = contBarbeiroCadeira;
	operacoes->contSofa = contSofa;
	operacoes->contLimiteCliente = contLimiteCliente;
	operacoes->filaSofa = filaSofa;
	operacoes->filaEmPe = filaEmPe;
	operacoes->filaPagamento = filaPagamento;
	operacoes->todoAtendidos = 0;
	operacoes->totalAtendidoPorTodos = 0;

}

// @ inicia fila;
void iniciaFila(P_fila fila, int i){
	fila -> limite = i;
	fila -> dado = (P_Cliente)malloc(i*sizeof(T_Cliente));
	fila -> primeiro = 0;
	fila -> ultimo = -1;
	fila -> quantItens = 0;

}

void inserirFila(P_fila fila, P_Cliente cliente){

	if(fila->ultimo == fila->limite-1){
		fila->ultimo = -1;
	}

	fila-> ultimo++;
	fila-> dado[fila->ultimo] = *(P_Cliente) cliente;
	fila-> quantItens++;
}


T_Cliente atendeClienteSofa(P_fila fila){

	T_Cliente aux = fila->dado[fila->primeiro++];

	if(fila->primeiro == fila->limite){
		fila->primeiro == 0;
	}

	fila->quantItens--;
	return aux;

}

void sentarNoSofa(P_fila filaSofa,P_fila filaEmPe){

	T_Cliente aux = filaEmPe->dado[filaEmPe->primeiro++];
	P_Cliente cliente = (P_Cliente)malloc(1*sizeof(T_Cliente));
	cliente->ident 	= aux.ident;
	inserirFila(filaSofa,cliente);
	if(filaEmPe->primeiro == filaEmPe->limite){
		filaEmPe->primeiro == 0;
	}
	filaEmPe->quantItens--;

}

void iniciaListaBarbeiro(P_Barbeiro lista[],P_operacoes operacoes){

	int quantBarbeiro = operacoes->contBarbeiroCadeira;
	int cont = 0;
	while(cont < quantBarbeiro){
		P_Barbeiro barb = (P_Barbeiro)malloc(1*sizeof(T_Barbeiro));
		barb->ident = cont;
		barb->quantidadeAtendimento = 0;
		barb->finalizou = 0;
		barb->dormindo = 1;
		barb->recebendoPagamento = 0;
		barb->operacoes = operacoes;
		lista[cont] = barb;
		cont++;
	}

}

int medirEspacoEmPeSalao(int max, int cont){

	int espaco = max - cont;
	if(espaco > 0){
		return espaco;
	}
	return 0;
}

int salaoEstaCheio(int contSofa, int contEmPE, int max){

	int contAtual = contSofa+contEmPE;
	if(contAtual < max){
		return 0;
	}
	return 1;
}

int sofaEstaCheio(int quantSofa, int limiteSofa){

	if(quantSofa < limiteSofa){
		return 0;
	}
	return 1; 
}

int antendeuMinimo(P_Barbeiro barb){

	if( barb->quantidadeAtendimento >= 
		barb->operacoes->contMinimaAtendimento){
		return 1;
	}
	return 0;	
}
void receberPagamento(P_fila fila){

	T_Cliente aux = fila->dado[fila->primeiro++];
	if(fila->primeiro == fila->limite){
		fila->primeiro == 0;
	}
	fila->quantItens--;
}

int estaVaziaFila(P_fila fila){

	return(fila->quantItens == 0);
}

int barbeiroDisponivel(P_Barbeiro lista[],int tam){

	for(int i = 0; i < tam;i++){
		if(lista[i]->dormindo == 1){
			return 1;
		}
	}
	return 0;
}

void esperaPagamento(P_fila filaSofa,P_fila filaPag){

	T_Cliente aux = filaPag->dado[filaPag->primeiro++];
	P_Cliente cliente = (P_Cliente)malloc(1*sizeof(T_Cliente));
	cliente->ident 	= aux.ident;
	inserirFila(filaSofa,cliente);
	if(filaPag->primeiro == filaPag->limite){
		filaPag->primeiro == 0;
	}
	filaPag->quantItens--;
}



void *threadClienteOperacao(void* param){

	P_Barbeiro * lista 	= (P_Barbeiro *) param;
	P_operacoes operacoes = lista[0]->operacoes;
	P_fila filaSofa = operacoes->filaSofa;
	P_fila filaEmPe = operacoes->filaEmPe;
	int contFilaEmPe = 0;
	int contFilaSofa = 0;
	int contLimiteCliente = operacoes->contLimiteCliente;
	
	while(1){

		// @ Chegou no salão.
	 	contFilaSofa = filaSofa->quantItens;
	 	contFilaEmPe = filaEmPe->quantItens;
	 	pthread_mutex_lock(operacoes->mutex_fila);
	 	pthread_mutex_lock(operacoes->mutex_barb);

	 	if(!(salaoEstaCheio(contFilaSofa,contFilaEmPe,contLimiteCliente))){

	 		pthread_mutex_unlock(operacoes->mutex_barb);
	 		P_Cliente cliente = (P_Cliente)malloc(1*sizeof(T_Cliente));
			cliente->ident 	= operacoes->totalAtendidoPorTodos + 1;

			operacoes->totalAtendidoPorTodos++;

	 		if(!sofaEstaCheio(contFilaSofa,filaSofa->limite)){
	 			inserirFila(filaSofa,cliente);
	 		}else{ 
	 			inserirFila(filaEmPe,cliente);
	 		}
	 		pthread_mutex_unlock(operacoes->mutex_fila);

	 	}else{

	 		pthread_mutex_unlock(operacoes->mutex_barb);
	 		pthread_mutex_unlock(operacoes->mutex_fila);
	 	}
	 	sem_post(operacoes->semaph_barbeiro);
	}
	
}

void *threadBarbeiro(void* param){

	P_Barbeiro barb= (P_Barbeiro)param;
	P_fila filaSofa = barb->operacoes->filaSofa;
	P_fila filaEmPe = barb->operacoes->filaEmPe;

	while (1){
		pthread_mutex_lock(barb->operacoes->mutex_fila);
		
		// @ Processo de Atentimendo
		if(!estaVaziaFila(filaSofa) && 
			!barb->operacoes->todoAtendidos){
			
			T_Cliente cliente = atendeClienteSofa(filaSofa);
			if(!estaVaziaFila(filaEmPe)){
				sentarNoSofa(filaSofa,filaEmPe);
			}
			pthread_mutex_unlock(barb->operacoes->mutex_fila);
			pthread_mutex_lock(barb->operacoes->mutex_barb);
			barb->dormindo = 0;
			barb->quantidadeAtendimento ++;
			if(antendeuMinimo(barb)){
				barb->finalizou = 1;
			}
			esperaPagamento(barb->operacoes->filaSofa,barb->operacoes->filaPagamento);
			pthread_mutex_unlock(barb->operacoes->mutex_barb);
			barb->dormindo = 1;

		}else{
			pthread_mutex_unlock(barb->operacoes->mutex_fila);
			pthread_mutex_unlock(barb->operacoes->mutex_barb);
		}
		// @ Processo de Pagamento
		pthread_mutex_lock(barb->operacoes->mutex_caixa);
		pthread_mutex_lock(barb->operacoes->mutex_barb);
		
		if(!estaVaziaFila(barb->operacoes->filaPagamento)){ 
			barb->dormindo = 0;
			barb->recebendoPagamento = 1; 
			receberPagamento(barb->operacoes->filaPagamento);
		}
		pthread_mutex_unlock(barb->operacoes->mutex_barb);
		pthread_mutex_unlock(barb->operacoes->mutex_caixa);
		barb->dormindo = 1;
		barb->recebendoPagamento = 0;
		sem_wait(barb->operacoes->semaph_barbeiro);

	}

	return 0;
}

int todoAtendidos(P_Barbeiro listaBarbeiro[],
	int quantBarbeiroCadeira,P_operacoes operacoes){

	if(quantBarbeiroCadeira > 0 ){

		for(int i = 0; i < quantBarbeiroCadeira;i++){
		
			if(listaBarbeiro[i]->finalizou == 0){
				return 0;
			}
		}
		operacoes->todoAtendidos = 1;
		return 1;
	}
}


int main45(int argc, char **argv){

	//@ Argumentos
	int contBarbeiroCadeira = atoi(argv[1]); 
	int contSofa = atoi(argv[2]); 
	int contLimiteCliente = atoi(argv[3]); 
	int contMinimaAtendimento = atoi(argv[4]); 

	int result = 0;
	int situacaoParada = 1; 
	int quantThreads = contBarbeiroCadeira;
	int quantEmPE = medirEspacoEmPeSalao(contLimiteCliente,contSofa);
	pthread_mutex_t mutex_fila;
	pthread_mutex_t mutex_barb;
	pthread_mutex_t mutex_caixa;
	sem_t semaph_barbeiro;
	P_operacoes operacoes = (P_operacoes)malloc(1*sizeof(T_operacoes));
	P_fila filaEmPe = (P_fila)malloc(1*sizeof(T_fila));
	P_fila filaSofa	= (P_fila)malloc(1*sizeof(T_fila));
	P_fila filaPagamento = (P_fila)malloc(1*sizeof(T_fila));

	iniciaOperacoes(operacoes,contBarbeiroCadeira,contSofa, contMinimaAtendimento,
		contLimiteCliente,mutex_fila,mutex_barb,mutex_caixa,semaph_barbeiro,
		filaSofa,filaEmPe,filaPagamento);

	P_Barbeiro 		listaBarbeiro[contSofa];
	
	iniciaListaBarbeiro(listaBarbeiro,operacoes);
	iniciaFila(filaSofa,contSofa);
	iniciaFila(filaEmPe,quantEmPE);
	iniciaFila(filaPagamento,contSofa);
	operacoes->filaEmPe = filaEmPe;
	operacoes->filaSofa = filaSofa;
	operacoes->filaPagamento  = filaPagamento;

	sem_init(operacoes->semaph_barbeiro,0,0);
    pthread_t * vetor_threads = malloc(quantThreads*sizeof(pthread_t));

    for(long cont = 0; cont < quantThreads;cont++){
    	result = pthread_create(&vetor_threads[cont],NULL,threadBarbeiro,(void*)listaBarbeiro[cont]);
    	if(cont == quantThreads - 1){
    		result = pthread_create(&vetor_threads[cont],NULL,threadClienteOperacao,(void*)listaBarbeiro);
    	}

    	if(result != 0){
            printf("Error em criar thread.");
            exit(-1);
        }   

    }

    while(situacaoParada == 1){
		situacaoParada = !todoAtendidos(listaBarbeiro,contBarbeiroCadeira,operacoes);
    }

    // @ Exibi Resultado
	for(int i = 0; i< contBarbeiroCadeira;i++){
		printf("barbeiro %d atendeu %d\n",listaBarbeiro[i]->ident,listaBarbeiro[i]->quantidadeAtendimento);
	}

    free(filaEmPe);
    free(filaPagamento);
    free(filaSofa);
    free(operacoes);
    free(vetor_threads);
    sem_destroy(&semaph_cont);
}