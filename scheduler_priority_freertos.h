#ifndef SCHEDULER_PRIORITY_FREERTOS_H
#define SCHEDULER_PRIORITY_FREERTOS_H

#include "ready_queue.h"

/*
 * Ejecuta un paso del calendarizador por prioridad sobre una cola.
 *
 * Regla usada:
 * - Menor numero de prioridad = mayor prioridad.
 *
 * Ejemplo:
 * priority = 1 corre antes que priority = 4.
 */
void priorityFreeRTOSStep(ReadyQueue *queue);

/*
 * Ejecuta una simulacion de prioridad usando dos colas:
 * - izquierda
 * - derecha
 *
 * Por ahora ejecuta un paso por cada lado.
 * Mas adelante, el algoritmo de flujo del canal decidira
 * si toca izquierda, derecha, equidad, letrero o tico.
 */
void runPriorityFreeRTOSTwoQueues(
    ReadyQueue *left_queue,
    ReadyQueue *right_queue
);

#endif