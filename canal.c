#include <stdio.h>
#include <stdlib.h>
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

/*
 * NUEVO: Función para devolver el barco a la fila de forma ordenada
 * dependiendo del algoritmo expropiativo que esté corriendo.
 */
static void reenqueue_ship(ReadyQueue *queue, ShipTask *ship, SchedulerType scheduler) {
    if (scheduler == SCHEDULER_STRN) {
        // Lógica de inserción ordenada para STRN (menor remaining_time primero)
        ReadyNode *new_node = (ReadyNode *)malloc(sizeof(ReadyNode));
        if (new_node == NULL) {
            printf("[ERROR] No se pudo asignar memoria para reencolar %s\n", ship->name);
            return;
        }
        new_node->ship = ship;
        new_node->next = NULL;

        // Si la cola está vacía
        if (queue->front == NULL) {
            queue->front = new_node;
            queue->rear = new_node;
            queue->size++;
            return;
        }

        // Si tiene menor tiempo restante que el primero
        if (ship->remaining_time < queue->front->ship->remaining_time) {
            new_node->next = queue->front;
            queue->front = new_node;
            queue->size++;
            return;
        }

        // Buscar la posición correcta para mantener el orden
        ReadyNode *current = queue->front;
        while (current->next != NULL && current->next->ship->remaining_time <= ship->remaining_time) {
            current = current->next;
        }

        new_node->next = current->next;
        current->next = new_node;

        if (new_node->next == NULL) {
            queue->rear = new_node;
        }
        queue->size++;
        
    } else {
        // RR u otros métodos usan la inserción normal al final de la fila
        enqueue(queue, ship);
    }
}

void run_channel_equity(ReadyQueue *left_queue, ReadyQueue *right_queue, int W, int max_ticks, SchedulerType active_scheduler) {
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
                lcd_display_update(left_queue, right_queue, current_ship);

                if (max_ticks == 0) {
                    // REGLA NO EXPROPIATIVA
                    while (!isShipFinished(current_ship)) {
                        wakeShipTask(current_ship);
                        wait_one_tick(current_ship);
                    }
                } else {
                    // REGLA EXPROPIATIVA
                    for (int i = 0; i < max_ticks; i++) {
                        if (isShipFinished(current_ship)) break;
                        wakeShipTask(current_ship);
                        wait_one_tick(current_ship);
                    }
                    
                    // MODIFICADO: Reencolado dinámico basado en el algoritmo
                    if (!isShipFinished(current_ship)) {
                        printf("[CALENDARIZADOR] %s agotó su tiempo. Vuelve a la cola de forma ordenada.\n", current_ship->name);
                        reenqueue_ship(active_queue, current_ship, active_scheduler); 
                    }
                }
                
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
    
    lcd_display_update(left_queue, right_queue, NULL);
    printf("\n[CANAL] Todas las colas están vacías. El puente está inactivo.\n");
}