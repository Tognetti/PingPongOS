#include "ppos.h"
#include "queue.h"
#include <stdio.h>
#include <stdlib.h>
#include <ucontext.h>

#define STACKSIZE 32768		/* tamanho de pilha das threads */

task_t task_main, *task_atual, task_dispatcher, *fila_tarefas, *task_next;
int id = 0;

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

  task_main.prev = NULL;
  task_main.next = NULL;
  task_main.tid = 0; //o id da main é 0
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

     //se a tarefa for de usuário (id > 1), coloca na fila e seta prioridade 0
     if ((task->tid) > 1) {
      queue_append ((queue_t **) &fila_tarefas, (queue_t*) task);
      task_setprio(task, 0);
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
  #ifdef DEBUG
  printf ("task_exit: tarefa %d sendo encerrada\n", task_atual->tid) ;
  #endif
  if (task_atual->tid == 1){
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
