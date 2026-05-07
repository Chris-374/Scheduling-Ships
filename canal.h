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

/* Enumerador para elegir el tipo de control del canal */
typedef enum {
    CHANNEL_EQUITY = 0,
    CHANNEL_SIGN = 1,
    CHANNEL_TICO = 2
} ChannelType;

/*
 * Ejecuta el canal usando una politica de flujo y un calendarizador.
 * param:
 *   - Equidad: W
 *   - Letrero: duracion del letrero en ticks
 *   - Tico: cualquier valor > 0
 * max_ticks:
 *   - 0 = no expropiativo, el barco sigue hasta terminar su trabajo
 *   - N = expropiativo, tras N unidades vuelve a cola guardando posicion
 */
void run_channel_flow(
    ChannelType channel_type,
    ReadyQueue *left_queue,
    ReadyQueue *right_queue,
    int param,
    int max_ticks,
    SchedulerType active_scheduler
);

#endif