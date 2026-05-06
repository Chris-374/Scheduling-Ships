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

/* Función para devolver el barco a la fila ordenadamente (Soporta STRN) */
static void reenqueue_ship(ReadyQueue *queue, ShipTask *ship, SchedulerType scheduler) {
    if (scheduler == SCHEDULER_STRN) {
        ReadyNode *new_node = (ReadyNode *)malloc(sizeof(ReadyNode));
        if (new_node == NULL) return;
        new_node->ship = ship;
        new_node->next = NULL;

        if (queue->front == NULL) {
            queue->front = new_node;
            queue->rear = new_node;
            queue->size++;
            return;
        }

        if (ship->remaining_time < queue->front->ship->remaining_time) {
            new_node->next = queue->front;
            queue->front = new_node;
            queue->size++;
            return;
        }

        ReadyNode *current = queue->front;
        while (current->next != NULL && current->next->ship->remaining_time <= ship->remaining_time) {
            current = current->next;
        }

        new_node->next = current->next;
        current->next = new_node;

        if (new_node->next == NULL) queue->rear = new_node;
        queue->size++;
        
    } else {
        enqueue(queue, ship);
    }
}

/* =========================================
 * POLÍTICA 1: EQUIDAD (W)
 * ========================================= */
static void run_equity(ReadyQueue *left_queue, ReadyQueue *right_queue, int W, int max_ticks, SchedulerType active_scheduler) {
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
                    while (!isShipFinished(current_ship)) {
                        wakeShipTask(current_ship);
                        wait_one_tick(current_ship);
                    }
                } else {
                    for (int i = 0; i < max_ticks; i++) {
                        if (isShipFinished(current_ship)) break;
                        wakeShipTask(current_ship);
                        wait_one_tick(current_ship);
                    }
                    if (!isShipFinished(current_ship)) {
                        printf("[CALENDARIZADOR] %s agotó su tiempo. Vuelve a la cola ordenada.\n", current_ship->name);
                        reenqueue_ship(active_queue, current_ship, active_scheduler); 
                    }
                }
                ships_passed++;
            }
        } else {
            current_side = (current_side == LEFT_SIDE) ? RIGHT_SIDE : LEFT_SIDE;
            ships_passed = 0;
            printf("\n[CANAL] Cambio de sentido. Atendiendo a la cola %s.\n", (current_side == LEFT_SIDE) ? "Izquierda" : "Derecha");
            vTaskDelay(pdMS_TO_TICKS(100)); 
        }
    }
}

/* =========================================
 * NUEVA POLÍTICA 2: LETRERO (Temporizador)
 * ========================================= */
static void run_sign(ReadyQueue *left_queue, ReadyQueue *right_queue, int sign_duration, int max_ticks, SchedulerType active_scheduler) {
    int current_side = LEFT_SIDE;
    int elapsed_time = 0; // Tiempo transcurrido con el letrero actual
    ShipTask *current_ship = NULL;

    printf("\n[CANAL] Iniciando control de flujo: LETRERO (Tiempo = %d unidades)\n", sign_duration);

    while (!isQueueEmpty(left_queue) || !isQueueEmpty(right_queue)) {
        ReadyQueue *active_queue = (current_side == LEFT_SIDE) ? left_queue : right_queue;
        const char *side_name = (current_side == LEFT_SIDE) ? "Izquierda" : "Derecha";

        // Revisamos si ya es hora de cambiar el letrero
        if (elapsed_time >= sign_duration) {
            current_side = (current_side == LEFT_SIDE) ? RIGHT_SIDE : LEFT_SIDE;
            elapsed_time = 0; // Reiniciamos el temporizador
            printf("\n[CANAL] ¡Temporizador expiró! El letrero cambió hacia la %s.\n", (current_side == LEFT_SIDE) ? "Izquierda" : "Derecha");
            continue;
        }

        if (!isQueueEmpty(active_queue)) {
            if (dequeue(active_queue, &current_ship)) {
                printf("\n[CANAL] %s entra al canal desde la %s.\n", current_ship->name, side_name);
                lcd_display_update(left_queue, right_queue, current_ship);

                if (max_ticks == 0) {
                    while (!isShipFinished(current_ship)) {
                        wakeShipTask(current_ship);
                        wait_one_tick(current_ship);
                        elapsed_time++; // Se cuenta el tiempo físico
                    }
                } else {
                    for (int i = 0; i < max_ticks; i++) {
                        if (isShipFinished(current_ship)) break;
                        wakeShipTask(current_ship);
                        wait_one_tick(current_ship);
                        elapsed_time++; // Se cuenta el tiempo físico
                    }
                    if (!isShipFinished(current_ship)) {
                        printf("[CALENDARIZADOR] %s agotó su tiempo. Vuelve a la cola ordenada.\n", current_ship->name);
                        reenqueue_ship(active_queue, current_ship, active_scheduler); 
                    }
                }
            }
        } else {
            // Si la cola activa está vacía, el tiempo igual debe seguir pasando
            printf("[CANAL] La cola %s está vacía, pero su letrero sigue en verde... (esperando)\n", side_name);
            vTaskDelay(pdMS_TO_TICKS(500)); // Simulamos 1 unidad de tiempo
            elapsed_time++;
        }
    }
}
/* =========================================
 * NUEVA POLÍTICA 3: TICO (Pase hasta vaciar)
 * ========================================= */
static void run_tico(ReadyQueue *left_queue, ReadyQueue *right_queue, int max_ticks, SchedulerType active_scheduler) {
    int current_side = LEFT_SIDE;
    ShipTask *current_ship = NULL;

    printf("\n[CANAL] Iniciando control de flujo: TICO (Pasan hasta vaciar la fila)\n");

    while (!isQueueEmpty(left_queue) || !isQueueEmpty(right_queue)) {
        ReadyQueue *active_queue = (current_side == LEFT_SIDE) ? left_queue : right_queue;
        const char *side_name = (current_side == LEFT_SIDE) ? "Izquierda" : "Derecha";

        // Si hay barcos de este lado, pasan. Si no, cambiamos de lado inmediatamente.
        if (!isQueueEmpty(active_queue)) {
            if (dequeue(active_queue, &current_ship)) {
                printf("\n[CANAL] %s entra al canal desde la %s.\n", current_ship->name, side_name);
                lcd_display_update(left_queue, right_queue, current_ship);

                if (max_ticks == 0) {
                    while (!isShipFinished(current_ship)) {
                        wakeShipTask(current_ship);
                        wait_one_tick(current_ship);
                    }
                } else {
                    for (int i = 0; i < max_ticks; i++) {
                        if (isShipFinished(current_ship)) break;
                        wakeShipTask(current_ship);
                        wait_one_tick(current_ship);
                    }
                    if (!isShipFinished(current_ship)) {
                        printf("[CALENDARIZADOR] %s agotó su tiempo. Vuelve a la cola ordenada.\n", current_ship->name);
                        reenqueue_ship(active_queue, current_ship, active_scheduler); 
                    }
                }
            }
        } else {
            // No hay control de flujo estricto, cedemos el puente al lado que sí tiene barcos
            current_side = (current_side == LEFT_SIDE) ? RIGHT_SIDE : LEFT_SIDE;
            printf("\n[CANAL] La fila %s está vacía. Cediendo el paso al lado contrario.\n", side_name);
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }
}
/* =========================================
 * CONTROLADOR PRINCIPAL DEL CANAL
 * ========================================= */
void run_channel_flow(ChannelType channel_type, ReadyQueue *left_queue, ReadyQueue *right_queue, int param, int max_ticks, SchedulerType active_scheduler) {
    if (param <= 0) {
        printf("[ERROR CANAL] El parámetro de configuración debe ser mayor que 0.\n");
        return;
    }

    switch (channel_type) {
        case CHANNEL_EQUITY:
            run_equity(left_queue, right_queue, param, max_ticks, active_scheduler);
            break;
        case CHANNEL_SIGN:
            run_sign(left_queue, right_queue, param, max_ticks, active_scheduler);
            break;
        case CHANNEL_TICO:
            // NUEVO: Llamada a la función Tico
            run_tico(left_queue, right_queue, max_ticks, active_scheduler);
            break;
        default:
            printf("\n[ERROR] Algoritmo de canal desconocido.\n");
            break;
    }

    lcd_display_update(left_queue, right_queue, NULL);
    printf("\n[CANAL] Todas las colas están vacías. El puente está inactivo.\n");
}