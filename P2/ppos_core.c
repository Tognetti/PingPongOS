#include "ppos.h"
#include <stdio.h>
#include <stdlib.h>
#include <ucontext.h>

#define STACKSIZE 32768		/* tamanho de pilha das threads */

task_t task_main, *task_atual;
int id = 0;

void ppos_init () {
  /* desativa o buffer da saida padrao (stdout), usado pela função printf */
  setvbuf (stdout, 0, _IONBF, 0) ;

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
  task_switch(&task_main);
}

int task_id () {
 return task_atual->tid;
}
