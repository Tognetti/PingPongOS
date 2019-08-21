#include <stdio.h>
#include <stdlib.h>
#include "ppos.h"

//O sistema implementado deve ter 3 produtores e 2 consumidores

int item; // entre 0 e 99
int buffer[5];
int i = 0;
semaphore_t s_buffer, s_item, s_vaga;
task_t p1, p2, p3, c1, c2;

void produtor () {

  while (1) {
      task_sleep (1);
      item = rand() % 100;

      sem_down (&s_vaga);

      sem_down (&s_buffer);
      buffer[i] = item;
      printf("Produzi %d\n",buffer[i]);
      i++;
      sem_up (&s_buffer);

      sem_up (&s_item);
  }
}

void consumidor () {
  while (1) {
      sem_down (&s_item);

      sem_down (&s_buffer);
      item = buffer[i];
      i--;
      sem_up (&s_buffer);

      sem_up (&s_vaga);

      printf("                 Consumi %d\n",item);
      task_sleep (1);
  }
}

int main () {

  ppos_init ();

  sem_create (&s_buffer, 1);
  sem_create (&s_item, 0);
  sem_create (&s_vaga, 5);

  task_create (&p1, produtor, "P1");
  task_create (&p2, produtor, "P2");
  task_create (&p3, produtor, "P3");
  task_create (&c1, consumidor, "C1");
  task_create (&c2, consumidor, "C2");

  task_join (&c1) ;

  task_yield ();

  exit (0);
}
