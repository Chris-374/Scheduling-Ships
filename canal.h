#ifndef CANAL_H
#define CANAL_H

#include "ready_queue.h"

/* 
 * Enumerador de calendarizadores (Movido desde main.c)
 */
typedef enum {
    SCHEDULER_RR = 0,
    SCHEDULER_PRIORITY = 1,
    SCHEDULER_SJF = 2,
    SCHEDULER_STRN = 3,
    SCHEDULER_FCFS = 4,
    SCHEDULER_EDF = 5
} SchedulerType;

/* 
 * W: Cantidad de barcos que pasan por lado.
 * max_ticks: Cuánto tiempo cruza un barco (0 = cruce completo, N = quantum de RR o STRN)
 * active_scheduler: Para saber cómo reencolar si el barco es expropiado.
 */
void run_channel_equity(ReadyQueue *left_queue, ReadyQueue *right_queue, int W, int max_ticks, SchedulerType active_scheduler);

#endif