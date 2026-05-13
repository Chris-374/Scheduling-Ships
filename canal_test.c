#include <stdio.h>
#include <unistd.h> // Para usar sleep() en Linux
#include "scheduler.h"

void run_channel_equity(ReadyQueue *left_queue, ReadyQueue *right_queue, int W) {
    if (W <= 0) {
        printf("[ERROR CANAL] El parámetro W debe ser mayor que 0.\n");
        return;
    }

    int current_side = LEFT_SIDE;
    int ships_passed = 0;
    Task current_ship; // Usamos el Task normal de tu C, no el ShipTask de FreeRTOS

    printf("\n[CANAL] Iniciando control de flujo: EQUIDAD (W = %d)\n", W);

    while (!isQueueEmpty(left_queue) || !isQueueEmpty(right_queue)) {
        ReadyQueue *active_queue = (current_side == LEFT_SIDE) ? left_queue : right_queue;
        const char *side_name = (current_side == LEFT_SIDE) ? "Izquierda" : "Derecha";

        if (!isQueueEmpty(active_queue) && ships_passed < W) {
            
            // Sacamos el barco de la cola
            if (dequeue(active_queue, &current_ship)) {
                printf("\n[CANAL] %s entra al canal desde la %s.\n", current_ship.name, side_name);
                printf("        (Simulando que cruza el puente...)\n");
                
                // Pausa de 1 segundo para simular que está cruzando físicamente
                sleep(1); 
                
                printf("[CANAL] %s ha terminado de cruzar exitosamente.\n", current_ship.name);
                ships_passed++;
            }
        } 
        else {
            current_side = (current_side == LEFT_SIDE) ? RIGHT_SIDE : LEFT_SIDE;
            ships_passed = 0;
            printf("\n[CANAL] Cambio de sentido. Atendiendo a la cola %s.\n", 
                   (current_side == LEFT_SIDE) ? "Izquierda" : "Derecha");
            
            // Pequeña pausa al cambiar de letrero/sentido
            usleep(500000); // Medio segundo
        }
    }

    printf("\n[CANAL] Todas las colas están vacías. El puente está inactivo.\n");
}