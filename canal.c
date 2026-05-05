#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "ready_queue.h"
#include "ship_tasks.h"
#include "canal.h"
#include "lcd_display.h" // Se añade para actualizar la pantalla física

static void wait_for_ship_to_cross(ShipTask *ship) {
    if (ship == NULL) return;
    while (ship->state == SHIP_RUNNING) {
        vTaskDelay(pdMS_TO_TICKS(50));
    }
    vTaskDelay(pdMS_TO_TICKS(50)); 
}

void run_channel_equity(ReadyQueue *left_queue, ReadyQueue *right_queue, int W) {
    if (W <= 0) {
        printf("[ERROR CANAL] El parámetro W debe ser mayor que 0.\n");
        return;
    }

    int current_side = LEFT_SIDE;
    int ships_passed = 0;
    ShipTask *current_ship = NULL;

    printf("\n[CANAL] Iniciando control de flujo: EQUIDAD (W = %d)\n", W);

    while (!isQueueEmpty(left_queue) || !isQueueEmpty(right_queue)) {
        ReadyQueue *active_queue = (current_side == LEFT_SIDE) ? left_queue : right_queue;
        const char *side_name = (current_side == LEFT_SIDE) ? "Izquierda" : "Derecha";

        if (!isQueueEmpty(active_queue) && ships_passed < W) {
            
            if (dequeue(active_queue, &current_ship)) {
                printf("\n[CANAL] %s entra al canal desde la %s.\n", current_ship->name, side_name);
                
                // Actualizamos la pantalla LCD mostrando el barco que cruza
                lcd_display_update(left_queue, right_queue, current_ship);

                wakeShipTask(current_ship);
                wait_for_ship_to_cross(current_ship);
                
                ships_passed++;
            }
        } else {
            current_side = (current_side == LEFT_SIDE) ? RIGHT_SIDE : LEFT_SIDE;
            ships_passed = 0;
            printf("\n[CANAL] Cambio de sentido. Atendiendo a la cola %s.\n", 
                   (current_side == LEFT_SIDE) ? "Izquierda" : "Derecha");
            vTaskDelay(pdMS_TO_TICKS(100)); 
        }
    }
    
    // Al terminar, limpiamos el barco del canal en la pantalla
    lcd_display_update(left_queue, right_queue, NULL);
    printf("\n[CANAL] Todas las colas están vacías. El puente está inactivo.\n");
}