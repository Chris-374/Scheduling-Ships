#include <stdio.h>
#include "scheduler.h"
#include "scheduler_rr.h"

int main(void) {
    ReadyQueue left_queue;
    ReadyQueue right_queue;

    int quantum = 2;

    initQueue(&left_queue, "COLA IZQUIERDA", MAX_QUEUE_SIZE);
    initQueue(&right_queue, "COLA DERECHA", MAX_QUEUE_SIZE);

    /* Carga de prueba: maximo 4 barcos por lado, segun la idea del enunciado. */
    enqueue(&left_queue, createTask(1, "L1", NORMAL, LEFT_SIDE, 5));
    enqueue(&left_queue, createTask(2, "L2", FISHING, LEFT_SIDE, 3));
    enqueue(&left_queue, createTask(3, "L3", PATROL, LEFT_SIDE, 4));

    enqueue(&right_queue, createTask(4, "R1", NORMAL, RIGHT_SIDE, 6));
    enqueue(&right_queue, createTask(5, "R2", FISHING, RIGHT_SIDE, 2));
    enqueue(&right_queue, createTask(6, "R3", PATROL, RIGHT_SIDE, 5));

    printf("Estado inicial de las colas:\n");
    printQueue(&left_queue);
    printQueue(&right_queue);

    runRoundRobinTwoQueues(&left_queue, &right_queue, quantum);

    destroyQueue(&left_queue);
    destroyQueue(&right_queue);

    return 0;
}
