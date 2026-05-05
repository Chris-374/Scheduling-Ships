#ifndef SCHEDULER_FCFS_FREERTOS_H
#define SCHEDULER_FCFS_FREERTOS_H

#include "ready_queue.h"

/*
 * Ejecuta un paso del calendarizador FCFS sobre una cola.
 *
 * FCFS = First Come First Served.
 *
 * Regla usada:
 * - El primer barco que entro a la cola se ejecuta primero.
 * - El barco corre hasta terminar.
 */
void fcfsFreeRTOSStep(ReadyQueue *queue);

/*
 * Ejecuta una simulacion FCFS usando dos colas:
 * - izquierda
 * - derecha
 *
 * Por ahora ejecuta un barco de cada lado por ciclo.
 * Mas adelante, el algoritmo de flujo del canal decidira
 * si toca izquierda, derecha, equidad, letrero o tico.
 */
void runFCFSFreeRTOSTwoQueues(
    ReadyQueue *left_queue,
    ReadyQueue *right_queue
);

#endif