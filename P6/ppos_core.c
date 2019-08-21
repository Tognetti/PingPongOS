#include "ppos.h"
#include "queue.h"
#include <stdio.h>
#include <stdlib.h>
#include <ucontext.h>
#include <sys/time.h>
#include <signal.h>

#define STACKSIZE 32768		/* tamanho de pilha das threads */

task_t task_main, *task_atual, task_dispatcher, *fila_tarefas, *task_next;
int id = 0;
int time = 0;
int quantum, tempoComecoTarefa, tempoFimTarefa, tempoExit;

// estrutura que define um tratador de sinal (deve ser global ou static)
struct sigaction action;

// estrutura de inicialização to timer
struct itimerval timer;

unsigned int systime () {
  return time;
}

// tratador do sinal
void tratador (int signum) {
  time++; //a cada milisegundo a variável é incrementada
  if (task_atual->categoria) {
    if(quantum == 0) {
      task_yield();
    } else {
      quantum--;
    }
  }
}

task_t *scheduler() {
  if (queue_size((queue_t*) fila_tarefas) > 0) {
    task_t *task_prioridade = fila_tarefas;
    task_t *aux = fila_tarefas->next;

    //seleciona a tarefa com maior prioridade dinamica (escala negativa)
    do {
      if (aux->prioridade_dinamica < task_prioridade->prioridade_dinamica) {
        task_prioridade = aux;
      }
      aux = aux->next;
    } while(aux != fila_tarefas);

    /*a prioridade dinamica dessa tarefa e igualada a sua prioridade estatica
    e a prioridade dinamica das outras e somada ao fator de envelhecimento (-1)*/
    aux = fila_tarefas;
    do {
      if (aux->tid != task_prioridade->tid) {
        aux->prioridade_dinamica = aux->prioridade_dinamica + (-1);
      } else {
        task_prioridade->prioridade_dinamica = task_prioridade->prioridade;
      }
      aux = aux->next;
    } while (aux != fila_tarefas);

    return task_prioridade;
  } else {
    return 0;
  }
}

void dispatcher_body () { // dispatcher é uma tarefa
  while ( queue_size((queue_t*) fila_tarefas) > 0 ) {
    task_next = scheduler();
    if (task_next) {
       // ações antes de lançar a tarefa "next", se houverem
       quantum = 20;
       task_atual->activations++;
       tempoComecoTarefa = systime();
       queue_remove ((queue_t**) &fila_tarefas, (queue_t*) task_next) ;
       task_switch (task_next);
       // ações após retornar da tarefa "next", se houverem
    }
  }
  task_exit(0) ; // encerra a tarefa dispatcher
}

void ppos_init () {
  /* desativa o buffer da saida padrao (stdout), usado pela função printf */
  setvbuf (stdout, 0, _IONBF, 0) ;

  //criar a tarefa dispatcher usando a task_create
  task_create(&task_dispatcher, dispatcher_body, "DISPATCHER");
  task_dispatcher.categoria = 0; //diz que é uma tarefa do sistema
  task_dispatcher.activations = 0;
  task_dispatcher.tempoCreateTarefa = systime();
  task_dispatcher.processor_time = 0;

  task_main.prev = NULL;
  task_main.next = NULL;
  task_main.tid = 1;
  task_main.categoria = 1;
  task_main.activations = 0;
  task_dispatcher.tempoCreateTarefa = systime();
  task_dispatcher.processor_time = 0;
  task_atual = &task_main; //a primeira tarefa a ser iniciada é a main
}

int task_create (task_t *task, void (*start_func)(void *), void *arg) {
  getcontext (&task->contexto);
  char *stack ;
  stack = malloc (STACKSIZE);
  if (stack) {
     task->contexto.uc_stack.ss_sp = stack ;
     task->contexto.uc_stack.ss_size = STACKSIZE;
     task->contexto.uc_stack.ss_flags = 0;
     task->contexto.uc_link = 0;

     //incrementa e atribui o id para a nova tarefa
     id++;
     task->tid = id;

     //se a tarefa for de usuário, coloca na fila e seta prioridade 0
     if (task_atual != &task_dispatcher) {
      queue_append ((queue_t **) &fila_tarefas, (queue_t*) task);
      task_setprio(task, 0);
      task->categoria = 1; //diz que é uma tarefa de usuario
     }

     task->activations = 0;
     task->tempoCreateTarefa = systime();
     task->processor_time = 0;

     makecontext (&task->contexto, (void*)(*start_func), 1, arg);

     // registra a acao para o sinal de timer SIGALRM
     action.sa_handler = tratador ;
     sigemptyset (&action.sa_mask) ;
     action.sa_flags = 0 ;
     if (sigaction (SIGALRM, &action, 0) < 0) {
       perror ("Erro em sigaction: ") ;
       exit (1) ;
     }

     // ajusta valores do temporizador
     timer.it_value.tv_usec = 1000 ;      // primeiro disparo, em micro-segundos
     timer.it_value.tv_sec  = 0 ;      // primeiro disparo, em segundos
     timer.it_interval.tv_usec = 1000 ;   // disparos subsequentes, em micro-segundos
     timer.it_interval.tv_sec  = 0 ;   // disparos subsequentes, em segundos

     // arma o temporizador ITIMER_REAL (vide man setitimer)
     if (setitimer (ITIMER_REAL, &timer, 0) < 0) {
       perror ("Erro em setitimer: ") ;
       exit (1) ;
     }

     #ifdef DEBUG
     printf ("task_create: criou tarefa %d\n", task->tid) ;
     #endif
     return(task->tid); //retorna o id da task criada
  }
  else {
     return -1; //retorna valor negativo caso erro
  }
}

int task_switch (task_t *task) {
  task_t *aux;
  aux = task_atual;
  task_atual = task;
  #ifdef DEBUG
  printf ("task_switch: trocando tarefa %d -> %d\n", task_atual->tid, task->tid) ;
  #endif
  swapcontext (&aux->contexto, &task->contexto);
  return 0;
}

void task_exit (int exit_code) {
  #ifdef DEBUG
  printf ("task_exit: tarefa %d sendo encerrada\n", task_atual->tid) ;
  #endif

  tempoFimTarefa = systime();
  tempoExit = tempoFimTarefa - task_atual->tempoCreateTarefa;
  printf("Task %d exit: execution time %d ms, processor time %d ms, %d activations\n",task_atual->tid,tempoExit,task_atual->processor_time,task_atual->activations);

  if (task_atual == &task_dispatcher){
		task_switch(&task_main);
	} else {
    queue_remove ((queue_t**) &fila_tarefas, (queue_t*) task_atual);
    task_switch(&task_dispatcher);
  }
}

int task_id () {
 return task_atual->tid;
}

void task_yield () {
  task_atual->activations++;
  tempoFimTarefa = systime();
  task_atual->processor_time += tempoFimTarefa - tempoComecoTarefa;
  tempoComecoTarefa = 0;
  tempoFimTarefa = 0;
  if (task_atual->tid > 1) {
    queue_append((queue_t **) &fila_tarefas, (queue_t*) task_atual);
  }
  task_switch(&task_dispatcher);
}

void task_setprio (task_t *task, int prio) {
  if (task == NULL){
    task_atual->prioridade = prio;
    task_atual->prioridade_dinamica = prio;
  } else {
    task->prioridade = prio;
    task->prioridade_dinamica = prio;
  }
}

int task_getprio (task_t *task) {
  int aux;
  if (task == NULL) {
    aux = task_atual->prioridade;
  } else {
    aux = task->prioridade;
  }
  return aux;
}
