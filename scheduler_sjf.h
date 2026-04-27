#ifndef SCHEDULER_SJF_H
#define SCHEDULER_SJF_H

#include "scheduler.h"

/*
 * Calendarizador SJF: Shortest Job First.
 * Regla: el barco con menor burst_time pasa primero.
 * Si dos barcos tienen el mismo burst_time, se conserva el orden de llegada.
 */
int enqueueBySJF(ReadyQueue *queue, Task task);
void sjfStep(ReadyQueue *queue);
void runSJFTwoQueues(ReadyQueue *left_queue, ReadyQueue *right_queue);

#endif
