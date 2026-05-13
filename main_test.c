#include <stdio.h>
#include "scheduler.h"

// Declaramos la función que acabamos de crear
void run_channel_equity(ReadyQueue *left_queue, ReadyQueue *right_queue, int W);

int main() {
    ReadyQueue left_queue, right_queue;
    
    // Inicializamos las colas normales
    initQueue(&left_queue, "Cola Izquierda", 10);
    initQueue(&right_queue, "Cola Derecha", 10);

    // Creamos unos barcos de prueba
    Task b1 = createTask(1, "L1_Normal", NORMAL, LEFT_SIDE, 5);
    Task b2 = createTask(2, "L2_Patrulla", PATROL, LEFT_SIDE, 3);
    Task b3 = createTask(3, "L3_Pesquera", FISHING, LEFT_SIDE, 2);

    Task b4 = createTask(4, "R1_Normal", NORMAL, RIGHT_SIDE, 4);
    Task b5 = createTask(5, "R2_Pesquera", FISHING, RIGHT_SIDE, 3);

    // Los metemos a las colas simulando que llegaron en ese orden
    enqueue(&left_queue, b1);
    enqueue(&left_queue, b2);
    enqueue(&left_queue, b3);

    enqueue(&right_queue, b4);
    enqueue(&right_queue, b5);

    printf("=== PRUEBA AISLADA DEL CANAL EN LINUX ===\n");
    printQueue(&left_queue);
    printQueue(&right_queue);

    // Ejecutamos la lógica de equidad con W = 2
    run_channel_equity(&left_queue, &right_queue, 2);

    return 0;
}