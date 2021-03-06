#include "ppos.h"
#include "queue.h"
#include <stdio.h>
#include <stdlib.h>
#include <ucontext.h>
#include <sys/time.h>
#include <signal.h>

#define STACKSIZE 32768		/* tamanho de pilha das threads */

task_t task_main, *task_atual, task_dispatcher, *fila_tarefas, *task_next, *fila_dormindo;
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
  if (task_atual->categoria) { //caso seja tarefa de usuário
    if(quantum <= 0) {
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

void dispatcher_body () {
  task_t *aux_task, *temp_task;
  int tempoAtual;
  while ( (queue_size((queue_t*) fila_tarefas) > 0) || (queue_size((queue_t*) fila_dormindo) > 0)) {

    //verifica se alguma tarefa deve acordar
    if((queue_size((queue_t*) fila_dormindo) > 0)){
      aux_task = fila_dormindo;
      do {
        temp_task = aux_task->next;
        tempoAtual = systime();
        if((aux_task->tempo_acordar <= tempoAtual) && (aux_task->status == SUSPENSA)){
          aux_task->status = PRONTA;
          queue_remove((queue_t **)&fila_dormindo, (queue_t *)aux_task);
          queue_append((queue_t **)&fila_tarefas, (queue_t *)aux_task);
        }
        aux_task = temp_task;
      } while (fila_dormindo != NULL && aux_task != fila_dormindo);
    }

    task_next = scheduler();
    if (task_next) {
       // ações antes de lançar a tarefa "next", se houverem
       quantum = 20;
       task_atual->activations++;
       tempoFimTarefa = systime();
       task_atual->processor_time += tempoFimTarefa - tempoComecoTarefa;
       tempoComecoTarefa = systime();
       task_next->activations++;
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
  fila_tarefas = NULL;
  fila_dormindo = NULL;

  //criar a tarefa dispatcher usando a task_create
  task_create(&task_dispatcher, dispatcher_body, "DISPATCHER");
  task_dispatcher.categoria = 0; //diz que é uma tarefa do sistema
  task_dispatcher.activations = 0;
  task_dispatcher.tempoCreateTarefa = systime();
  task_dispatcher.processor_time = 0;

  task_main.prev = NULL;
  task_main.next = NULL;
  task_main.joiningMe = NULL;
  task_main.tid = 0;
  task_main.categoria = 1;
  task_main.activations = 0;
  task_dispatcher.tempoCreateTarefa = systime();
  task_dispatcher.processor_time = 0;
  task_atual = &task_main; //a primeira tarefa a ser iniciada é a main
  queue_append((queue_t **) &fila_tarefas,(queue_t*) &task_main);

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

  task_switch(&task_dispatcher);
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
     task->activations = 0;
     task->tempoCreateTarefa = systime();
     task->processor_time = 0;

     //se a tarefa for de usuário, coloca na fila e seta prioridade 0
     if (task_atual != &task_dispatcher) {
      queue_append ((queue_t **) &fila_tarefas, (queue_t*) task);
      task_setprio(task, 0);
      task->categoria = 1; //diz que é uma tarefa de usuario
      task->status = PRONTA;
     }

     makecontext (&task->contexto, (void*)(*start_func), 1, arg);

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
  task_t *aux_task;
  #ifdef DEBUG
  printf ("task_exit: tarefa %d sendo encerrada\n", task_atual->tid) ;
  #endif

  tempoFimTarefa = systime();
  tempoExit = tempoFimTarefa - task_atual->tempoCreateTarefa;
  printf("Task %d exit: execution time %d ms, processor time %d ms, %d activations\n",task_atual->tid,tempoExit,task_atual->processor_time,task_atual->activations);

  while(task_atual->joiningMe) {
    aux_task = (task_t *) queue_remove ((queue_t**) &task_atual->joiningMe, (queue_t*) task_atual->joiningMe);
    aux_task->status = PRONTA;
    queue_append((queue_t **) &fila_tarefas, (queue_t*) aux_task);
  }

  task_atual->exitCode = exit_code;
  task_atual->status = TERMINADA;

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
  queue_append((queue_t **) &fila_tarefas, (queue_t*) task_atual);
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

int task_join (task_t *task) {
  int retorno;
  if (task->status != TERMINADA) {
    task_atual->status = SUSPENSA;
    queue_append((queue_t **) &task->joiningMe, (queue_t*) task_atual);
    retorno = task->exitCode;
  } else {
    queue_append((queue_t **) &fila_tarefas, (queue_t*) task_atual);
    retorno = -1;
  }
  task_switch(&task_dispatcher);
  return retorno;
}

void task_sleep (int t) {
  task_atual->tempo_acordar = systime();
  task_atual->tempo_acordar = task_atual->tempo_acordar  + (t * 1000);
  task_atual->status = SUSPENSA;
  queue_append((queue_t **) &fila_dormindo, (queue_t*) task_atual);
  task_switch(&task_dispatcher);
}

int sem_create (semaphore_t *s, int value) {
  s->status = 0; // 0 = ativo, 1 = destruido
  s->value = value;
  s->semaphore_queue = NULL;
  return 0; //caso sucesso
}

int sem_down (semaphore_t *s) {
  if (s->status == 0) {
    s->value = s->value - 1;
    if (s->value < 0) {
      task_atual->status = SUSPENSA;
      queue_append((queue_t **) &(s->semaphore_queue), (queue_t*) task_atual);
      task_switch(&task_dispatcher);
    }
    return 0;
  } else {
    return -1;
  }
}

int sem_up (semaphore_t *s) {
  if (s->status == 0) {
    s->value = s->value + 1;
    if (s->value <= 0) {
      task_t *aux_task;
      aux_task = (task_t*) queue_remove ((queue_t**) &(s->semaphore_queue), (queue_t*) s->semaphore_queue);
      aux_task->status = PRONTA;
      queue_append((queue_t **) &fila_tarefas, (queue_t*) aux_task);
    }
    return 0;
  } else {
    return -1;
  }
}

int sem_destroy (semaphore_t *s) {
  task_t *aux_task;
  s->status = 1;
  while(s->semaphore_queue) {
    aux_task = (task_t *) queue_remove ((queue_t**) &s->semaphore_queue, (queue_t*) s->semaphore_queue);
    aux_task->status = PRONTA;
    queue_append((queue_t **) &fila_tarefas, (queue_t*) aux_task);
  }
  return 0;
}
