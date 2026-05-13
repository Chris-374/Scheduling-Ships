#ifndef SCHEDULER_STRN_FREERTOS_H
#define SCHEDULER_STRN_FREERTOS_H

#include "ready_queue.h"

/*
 * Ejecuta un paso del calendarizador STRN sobre una cola.
 *
 * STRN = Shortest Time Remaining Next.
 *
 * Regla usada:
 * - El barco con menor remaining_time se ejecuta primero.
 * - Solo ejecuta una unidad.
 * - Si no termina, vuelve a la cola.
 */
void strnFreeRTOSStep(ReadyQueue *queue);

/*
 * Ejecuta una simulacion STRN usando dos colas:
 * - izquierda
 * - derecha
 *
 * Por ahora ejecuta un paso por cada lado.
 * Mas adelante, el algoritmo de flujo del canal decidira
 * si toca izquierda, derecha, equidad, letrero o tico.
 */
void runSTRNFreeRTOSTwoQueues(
    ReadyQueue *left_queue,
    ReadyQueue *right_queue
);

#endif