#ifndef SCHEDULER_SJF_FREERTOS_H
#define SCHEDULER_SJF_FREERTOS_H

#include "ready_queue.h"

/*
 * Ejecuta un paso del calendarizador SJF sobre una cola.
 *
 * SJF significa Shortest Job First.
 *
 * Regla usada:
 * - El barco con menor burst_time se ejecuta primero.
 */
void sjfFreeRTOSStep(ReadyQueue *queue);

/*
 * Ejecuta una simulacion SJF usando dos colas:
 * - izquierda
 * - derecha
 *
 * Por ahora ejecuta un paso por cada lado.
 * Mas adelante, el algoritmo de flujo del canal decidira
 * si toca izquierda, derecha, equidad, letrero o tico.
 */
void runSJFFreeRTOSTwoQueues(
    ReadyQueue *left_queue,
    ReadyQueue *right_queue
);

#endif