#ifndef SCHEDULER_EDF_FREERTOS_H
#define SCHEDULER_EDF_FREERTOS_H

#include "ready_queue.h"

/*
 * Ejecuta un paso del calendarizador EDF sobre una cola.
 *
 * EDF = Earliest Deadline First.
 *
 * Regla usada:
 * - El barco con menor deadline se ejecuta primero.
 * - El barco corre hasta terminar.
 */
void edfFreeRTOSStep(ReadyQueue *queue);

/*
 * Ejecuta una simulacion EDF usando dos colas:
 * - izquierda
 * - derecha
 *
 * Por ahora ejecuta un barco de cada lado por ciclo.
 * Mas adelante, el algoritmo de flujo del canal decidira
 * si toca izquierda, derecha, equidad, letrero o tico.
 */
void runEDFFreeRTOSTwoQueues(
    ReadyQueue *left_queue,
    ReadyQueue *right_queue
);

#endif