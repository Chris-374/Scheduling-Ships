#ifndef SCHEDULER_FCFS_H
#define SCHEDULER_FCFS_H

#include "scheduler.h"

/*
 * FCFS no necesita una funcion de insercion especial.
 * Usa enqueue(), porque el primer barco que llega queda primero.
 */
void fcfsStep(ReadyQueue *queue);
void runFCFSTwoQueues(ReadyQueue *left_queue, ReadyQueue *right_queue);

#endif
