#ifndef SCHEDULER_RR_FREERTOS_H
#define SCHEDULER_RR_FREERTOS_H

#include "ready_queue.h"

/*
 * Ejecuta un paso de Round Robin sobre una cola.
 *
 * Recibe:
 * - queue: cola de listos.
 * - quantum: cantidad de unidades que puede avanzar el barco.
 *
 * En esta version, el barco es una task real de FreeRTOS.
 * Por eso el calendarizador no modifica directamente el tiempo restante,
 * sino que despierta la task del barco usando wakeShipTask().
 */
void roundRobinFreeRTOSStep(ReadyQueue *queue, int quantum);

/*
 * Ejecuta una simulacion RR usando dos colas:
 * - izquierda
 * - derecha
 *
 * Por ahora ejecuta un paso de RR por cada lado.
 * Mas adelante, el algoritmo de flujo del canal decidira
 * si toca izquierda, derecha, equidad, letrero o tico.
 */
void runRoundRobinFreeRTOSTwoQueues(
    ReadyQueue *left_queue,
    ReadyQueue *right_queue,
    int quantum
);

#endif