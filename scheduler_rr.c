#include <stdio.h>
#include "scheduler_rr.h"

void roundRobinStep(ReadyQueue *queue, int quantum) {
    if (queue == NULL) {
        return;
    }

    if (quantum <= 0) {
        printf("[ERROR] El quantum debe ser mayor que cero.\n");
        return;
    }

    if (isQueueEmpty(queue)) {
        printf("%s vacia. No hay barcos para calendarizar.\n", queue->name);
        return;
    }

    Task current_task;
    if (!dequeue(queue, &current_task)) {
        return;
    }

    int executed_time = quantum;
    if (current_task.remaining_time < quantum) {
        executed_time = current_task.remaining_time;
    }

    current_task.remaining_time -= executed_time;

    printf("RR en %s: ejecutando %s por %d unidad(es). Restante: %d.\n",
           queue->name,
           current_task.name,
           executed_time,
           current_task.remaining_time);

    if (current_task.remaining_time > 0) {
        enqueue(queue, current_task);
    } else {
        printf("%s termino y sale de la cola.\n", current_task.name);
    }
}

void runRoundRobinTwoQueues(ReadyQueue *left_queue, ReadyQueue *right_queue, int quantum) {
    int cycle = 1;

    printf("\n========== SIMULACION RR CON DOS COLAS ==========");
    printf("\nQuantum: %d\n", quantum);

    while (!isQueueEmpty(left_queue) || !isQueueEmpty(right_queue)) {
        printf("\n---------- Ciclo %d ----------\n", cycle);

        /* Por ahora se ejecuta un paso de RR por cada lado.
           Mas adelante, el algoritmo de flujo del canal decidira
           si toca izquierda, derecha, letrero, equidad, etc. */
        if (!isQueueEmpty(left_queue)) {
            roundRobinStep(left_queue, quantum);
        }

        if (!isQueueEmpty(right_queue)) {
            roundRobinStep(right_queue, quantum);
        }

        printQueue(left_queue);
        printQueue(right_queue);

        cycle++;
    }

    printf("\nTodas las colas quedaron vacias.\n");
}
