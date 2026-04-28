#ifndef SCHEDULER_STRN_H
#define SCHEDULER_STRN_H

#include "scheduler.h"

/*
 * Calendarizador STRN: Shortest Time Remaining Next.
 * Regla: el barco con menor remaining_time pasa primero.
 *
 * A diferencia de SJF, STRN se basa en el tiempo restante,
 * no necesariamente en el tiempo total original.
 * Esto permite que, si luego se agregan barcos mientras la simulacion corre,
 * siempre se pueda escoger el que tenga menos tiempo pendiente.
 */
int enqueueBySTRN(ReadyQueue *queue, Task task);
void strnStep(ReadyQueue *queue);
void runSTRNTwoQueues(ReadyQueue *left_queue, ReadyQueue *right_queue);

#endif
