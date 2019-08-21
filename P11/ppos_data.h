// PingPongOS - PingPong Operating System
// Prof. Carlos A. Maziero, DINF UFPR
// Versão 1.1 -- Julho de 2016
//
// Estruturas de dados internas do sistema operacional

#ifndef __PPOS_DATA__
#define __PPOS_DATA__

#include <ucontext.h>
#include "queue.h"

#define PRONTA 0
#define SUSPENSA 1
#define TERMINADA 2

// Estrutura que define uma tarefa
typedef struct task_t
{
  struct task_t *prev, *next ;   // para usar com a biblioteca de filas (cast)
  int tid ;                      // ID da tarefa
  ucontext_t contexto;
  int prioridade;                //prioridade de -20 até 20 em escala negativa
  int prioridade_dinamica;       //usado no envelhecimento
  int categoria;                 //0 para sistema, 1 para usuario
  int processor_time, activations;
  int tempoCreateTarefa;         //para guardar o momento em que a tarefa foi criada
  int exitCode;
  int status;                    // 1 para suspensa, 0 para pronta/executando, 2 para terminada
  struct task_t *joiningMe;      // um ponteiro para fila de tarefas que esperam esta tarefa
  int tempo_acordar;
} task_t ;

// estrutura que define um semáforo
typedef struct
{
  int value;
  struct task_t *semaphore_queue;
  int status; //0 para ativo, 1 para destruido
} semaphore_t ;

// estrutura que define um mutex
typedef struct
{
  // preencher quando for necessário
} mutex_t ;

// estrutura que define uma barreira
typedef struct
{
  // preencher quando for necessário
} barrier_t ;

// estrutura que define uma fila de mensagens
typedef struct
{
  // preencher quando for necessário
} mqueue_t ;

#endif
