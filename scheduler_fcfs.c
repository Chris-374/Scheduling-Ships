#include <stdio.h>
#include "scheduler_fcfs.h"

/*
 * Ejecuta un paso del calendarizador FCFS sobre una cola de listos.
 *
 * FCFS significa First Come, First Served.
 * En español: primero en llegar, primero en ser atendido.
 *
 * Este algoritmo no reordena la cola y tampoco usa quantum.
 * Simplemente toma el primer barco de la cola y lo ejecuta hasta terminar.
 */
void fcfsStep(ReadyQueue *queue) {
    if (queue == NULL) {
        return;
    }

    if (isQueueEmpty(queue)) {
        printf("%s vacia. No hay barcos para calendarizar.\n", queue->name);
        return;
    }

    /*
     * Como FCFS respeta el orden de llegada, siempre se toma
     * el barco que esta al frente de la cola.
     */
    Task current_task;
    if (!dequeue(queue, &current_task)) {
        return;
    }

    printf("FCFS en %s: ejecutando %s con tiempo %d hasta terminar.\n",
           queue->name,
           current_task.name,
           current_task.remaining_time);

    /*
     * En FCFS no hay expropiacion.
     * El barco que entra al canal termina su ejecucion completa.
     */
    current_task.remaining_time = 0;

    printf("%s termino y sale de la cola.\n", current_task.name);
}

/*
 * Ejecuta una simulacion FCFS usando dos colas de listos:
 * una para el lado izquierdo y otra para el lado derecho.
 *
 * Por ahora se ejecuta un paso de FCFS por cada cola en cada ciclo.
 * Mas adelante, el algoritmo de flujo del canal decidira si toca
 * izquierda, derecha, equidad, letrero o tico.
 */
void runFCFSTwoQueues(ReadyQueue *left_queue, ReadyQueue *right_queue) {
    int cycle = 1;

    printf("\n========== SIMULACION FCFS CON DOS COLAS ==========");
    printf("\nRegla: primero en llegar = primero en ser atendido.\n");

    while (!isQueueEmpty(left_queue) || !isQueueEmpty(right_queue)) {
        printf("\n---------- Ciclo %d ----------\n", cycle);

        if (!isQueueEmpty(left_queue)) {
            fcfsStep(left_queue);
        }

        if (!isQueueEmpty(right_queue)) {
            fcfsStep(right_queue);
        }

        printQueue(left_queue);
        printQueue(right_queue);

        cycle++;
    }

    printf("\nTodas las colas quedaron vacias.\n");
}
