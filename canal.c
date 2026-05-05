#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "canal.h"
#include "lcd_display.h"

static void wait_one_tick(ShipTask *ship) {
    if (ship == NULL) return;
    while (ship->state == SHIP_RUNNING) {
        vTaskDelay(pdMS_TO_TICKS(50));
    }
    vTaskDelay(pdMS_TO_TICKS(50)); 
}

void run_channel_equity(ReadyQueue *left_queue, ReadyQueue *right_queue, int W, int max_ticks) {
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
            
            // 1. El Canal saca al barco que el Calendarizador puso en la posición #1
            if (dequeue(active_queue, &current_ship)) {
                printf("\n[CANAL] %s entra al canal desde la %s.\n", current_ship->name, side_name);
                lcd_display_update(left_queue, right_queue, current_ship);

                // 2. El Canal revisa las reglas del Calendarizador para este barco
                if (max_ticks == 0) {
                    // REGLA NO EXPROPIATIVA (SJF, Prioridad, FCFS): Cruza completo
                    while (!isShipFinished(current_ship)) {
                        wakeShipTask(current_ship);
                        wait_one_tick(current_ship);
                    }
                } else {
                    // REGLA EXPROPIATIVA (Round Robin, STRN): Cruza solo por N unidades (quantum)
                    for (int i = 0; i < max_ticks; i++) {
                        if (isShipFinished(current_ship)) break;
                        wakeShipTask(current_ship);
                        wait_one_tick(current_ship);
                    }
                    
                    // Si el barco agotó su tiempo y no terminó, el calendarizador lo manda a hacer fila
                    if (!isShipFinished(current_ship)) {
                        printf("[CALENDARIZADOR] %s agotó su tiempo. Vuelve a la cola.\n", current_ship->name);
                        // Su compañero luego cambiará este enqueue genérico por enqueueByPriority si es STRN
                        enqueue(active_queue, current_ship); 
                    }
                }
                
                ships_passed++;
            }
        } else {
            // Lógica propia del Canal: Cambiar de lado cuando pasan W barcos
            current_side = (current_side == LEFT_SIDE) ? RIGHT_SIDE : LEFT_SIDE;
            ships_passed = 0;
            printf("\n[CANAL] Cambio de sentido. Atendiendo a la cola %s.\n", 
                   (current_side == LEFT_SIDE) ? "Izquierda" : "Derecha");
            vTaskDelay(pdMS_TO_TICKS(100)); 
        }
    }
    
    lcd_display_update(left_queue, right_queue, NULL);
    printf("\n[CANAL] Todas las colas están vacías. El puente está inactivo.\n");
}