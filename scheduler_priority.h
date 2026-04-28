#ifndef SCHEDULER_PRIORITY_H
#define SCHEDULER_PRIORITY_H

#include "scheduler.h"

/*
 * En este proyecto se usa la siguiente regla:
 * menor numero = mayor prioridad.
 * Si dos barcos tienen la misma prioridad, se conserva el orden de llegada.
 */
int enqueueByPriority(ReadyQueue *queue, Task task);
void priorityStep(ReadyQueue *queue);
void runPriorityTwoQueues(ReadyQueue *left_queue, ReadyQueue *right_queue);

#endif
