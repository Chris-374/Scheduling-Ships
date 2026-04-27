#include <stdio.h>
#include <stdlib.h>
#include "scheduler.h"
#include "scheduler_selector.h"

int main(int argc, char *argv[]) {
    SchedulerType scheduler_type = SCHEDULER_RR;
    int quantum = 2;

    if (argc >= 2) {
        scheduler_type = parseSchedulerType(argv[1]);
    } else {
        printf("Uso: %s <rr|priority|sjf|strn> [quantum]\n", argv[0]);
        printf("No se indico calendarizador. Se usara RR por defecto.\n");
    }

    if (argc >= 3) {
        quantum = atoi(argv[2]);
        if (quantum <= 0) {
            printf("[AVISO] Quantum invalido. Se usara quantum = 2.\n");
            quantum = 2;
        }
    }

    ReadyQueue left_queue;
    ReadyQueue right_queue;

    initQueue(&left_queue, "Cola izquierda", MAX_QUEUE_SIZE);
    initQueue(&right_queue, "Cola derecha", MAX_QUEUE_SIZE);

    /*
     * Carga de prueba.
     * En Prioridad se usa el campo priority.
     * Regla: menor numero = mayor prioridad.
     * En SJF se usa burst_time como el tiempo total de cada barco.
     * En STRN se usa remaining_time como el tiempo restante de cada barco.
     */
    insertTaskByScheduler(&left_queue,
                          createTaskWithPriority(1, "L1", NORMAL, LEFT_SIDE, 5, 5),
                          scheduler_type);

    insertTaskByScheduler(&left_queue,
                          createTaskWithPriority(2, "L2", PATROL, LEFT_SIDE, 3, 1),
                          scheduler_type);

    insertTaskByScheduler(&left_queue,
                          createTaskWithPriority(3, "L3", FISHING, LEFT_SIDE, 4, 3),
                          scheduler_type);

    insertTaskByScheduler(&right_queue,
                          createTaskWithPriority(4, "R1", NORMAL, RIGHT_SIDE, 6, 4),
                          scheduler_type);

    insertTaskByScheduler(&right_queue,
                          createTaskWithPriority(5, "R2", PATROL, RIGHT_SIDE, 2, 1),
                          scheduler_type);

    insertTaskByScheduler(&right_queue,
                          createTaskWithPriority(6, "R3", FISHING, RIGHT_SIDE, 4, 2),
                          scheduler_type);

    printf("\nEstado inicial de las colas:\n");
    printQueue(&left_queue);
    printQueue(&right_queue);

    runSchedulerTwoQueues(scheduler_type, &left_queue, &right_queue, quantum);

    destroyQueue(&left_queue);
    destroyQueue(&right_queue);

    return 0;
}
