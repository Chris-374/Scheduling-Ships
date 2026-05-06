#ifndef CANAL_H
#define CANAL_H

#include "ready_queue.h"

/* Enumerador de calendarizadores */
typedef enum {
    SCHEDULER_RR = 0,
    SCHEDULER_PRIORITY = 1,
    SCHEDULER_SJF = 2,
    SCHEDULER_STRN = 3,
    SCHEDULER_FCFS = 4,
    SCHEDULER_EDF = 5
} SchedulerType;

/* NUEVO: Enumerador para elegir el tipo de control del canal */
typedef enum {
    CHANNEL_EQUITY = 0,
    CHANNEL_SIGN = 1,
    CHANNEL_TICO = 2
} ChannelType;

/* 
 * Función unificada para correr cualquier política de canal.
 * param: Representa 'W' si es Equidad, o el 'tiempo del temporizador' si es Letrero.
 */
void run_channel_flow(ChannelType channel_type, ReadyQueue *left_queue, ReadyQueue *right_queue, int param, int max_ticks, SchedulerType active_scheduler);

#endif