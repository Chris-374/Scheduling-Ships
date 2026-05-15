#ifndef SCHEDULERS_SCHEDULER_POLICY_H
#define SCHEDULERS_SCHEDULER_POLICY_H

#include "canal.h"
#include "ready_queue.h"
#include "ship_tasks.h"

/*
 * Modulo de calendarizadores activos para el canal.
 *
 * canal.c controla la parte fisica:
 * - posiciones
 * - choques
 * - rebases
 * - sentido del canal
 * - flujo Equidad/Letrero/Tico
 *
 * Esta carpeta controla la politica de calendarizacion:
 * - RR
 * - FCFS
 * - Prioridad
 * - SJF
 * - STRN
 * - EDF
 */

/* Comparador comun usado por el canal. */
int scheduler_ship_goes_before(
    ShipTask *a,
    ShipTask *b,
    SchedulerType scheduler
);

/* Inserta un barco en la cola respetando el scheduler activo. */
int scheduler_enqueue_ordered(
    ReadyQueue *queue,
    ShipTask *ship,
    SchedulerType scheduler
);

/* Selecciona el siguiente barco sin removerlo todavia. */
ShipTask *scheduler_select_next_ship(
    ReadyQueue *queue,
    SchedulerType scheduler
);

/* Remueve un barco especifico de la cola. */
int scheduler_remove_specific_ship(
    ReadyQueue *queue,
    ShipTask *ship
);

const char *scheduler_to_string(SchedulerType scheduler);

/* Comparadores particulares. Cada uno vive en su propio .c. */
int scheduler_rr_goes_before(ShipTask *a, ShipTask *b);
int scheduler_fcfs_goes_before(ShipTask *a, ShipTask *b);
int scheduler_priority_goes_before(ShipTask *a, ShipTask *b);
int scheduler_sjf_goes_before(ShipTask *a, ShipTask *b);
int scheduler_strn_goes_before(ShipTask *a, ShipTask *b);
int scheduler_edf_goes_before(ShipTask *a, ShipTask *b);

#endif
