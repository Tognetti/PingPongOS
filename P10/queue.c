#include <stdio.h>
#include "queue.h"

void queue_append (queue_t **queue, queue_t *elem) {
  if (*queue == NULL) { //verifica se a lista é vazia
    *queue = elem;
    (*queue)->next = *queue;
    (*queue)->prev = *queue;
  } else {
    //verifica se o elemento não está em outra lista
    if((elem->next == NULL) && (elem->prev == NULL)) {
      queue_t *aux;
      aux = *queue;
      while (aux->next != *queue) { //deixa o apontador aux no último elemento
        aux = aux->next;
      }
      aux->next = elem;
      elem->prev = aux;
      elem->next = *queue;
      (*queue)->prev = elem;
    }
    /*else {
      printf("ERRO: O elemento está em outra lista\n");
    }*/
  }
}

queue_t *queue_remove (queue_t **queue, queue_t *elem) {
  if (*queue == NULL) { //verifica se a lista é vazia
    //A lista está vazia
    return NULL;
  } else {
    //verifica se o elemento está presente na lista
    int verifica_presente = 0;
    if (elem == *queue) {
      verifica_presente = 1;
    }
    queue_t *aux;
    aux = (*queue)->next;
    while (aux != *queue) {
      if (elem == aux) {
        verifica_presente = 1;
      }
      aux = aux->next;
    }

    if (verifica_presente == 1) {
      queue_t *ant, *prox;
      ant = elem->prev;
      prox = elem->next;
      ant->next = prox;
      prox->prev = ant;

      // caso o primeiro elemento for removido, o seguinte passa a ser o primeiro
      if (elem == *queue) {
        *queue = elem->next;
      }

      // verifica se só há um elemento (se o próximo é ele mesmo)
      if (elem->next == elem) {
        *queue = NULL; // a lista passa a ficar vazia
      }

      elem->next = NULL;
      elem->prev = NULL;
      return elem;
    } else {
      //O elemento não está na lista
      return NULL;
    }
  }
}

int queue_size (queue_t *queue) {
  queue_t *aux;
  int contador = 0;
  aux = queue;

  if (queue == NULL) { //verifica se a lista é vazia
    return 0;
  } else {
    do {
      contador++;
      aux = aux->next ;
    }
    while (aux != queue);
    return contador;
  }
}

void queue_print (char *name, queue_t *queue, void print_elem (void*) ) {
  printf("%s", name);

  if (queue != NULL) {
    print_elem(queue);
    printf(" ");
    queue_t *aux;
    aux = queue->next;
    while (aux != queue) {
      print_elem(aux);
      printf(" ");
      aux = aux->next;
    }
    printf("\n");
  } else {
    printf("[]\n");
  }
}
