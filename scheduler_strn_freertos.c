#include <stdio.h>
#include <stdlib.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "scheduler_strn_freertos.h"
#include "ship_tasks.h"

/*
 * Espera a que la task del barco termine una unidad de ejecucion.
 *
 * Version mejorada:
 * Ya no hace polling revisando ship->state.
 *
 * Ahora el scheduler queda bloqueado hasta que el barco le mande
 * una notificacion de regreso cuando termina una unidad.
 */
static void waitForShipExecution(ShipTask *ship) {
    if (ship == NULL) {
        return;
    }

    /*
     * El scheduler se bloquea aqui.
     * No consume CPU esperando en un while.
     *
     * El barco debe llamar xTaskNotifyGive(ship->scheduler_handle)
     * cuando termina su unidad de ejecucion.
     */
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
}

/*
 * Busca y remueve de la cola el barco con menor remaining_time.
 *
 * En esta implementacion:
 * - Menor remaining_time = corre primero.
 *
 * Esto representa STRN/SRTN.
 */
static int dequeueShortestRemainingTime(ReadyQueue *queue, ShipTask **ship_out) {
    if (queue == NULL || ship_out == NULL || queue->front == NULL) {
        return 0;
    }

    ReadyNode *current = queue->front;
    ReadyNode *previous = NULL;

    ReadyNode *best_node = queue->front;
    ReadyNode *best_previous = NULL;

    /*
     * Recorremos la cola buscando el barco con menor remaining_time.
     */
    while (current != NULL) {
        if (current->ship != NULL && best_node->ship != NULL) {
            if (current->ship->remaining_time < best_node->ship->remaining_time) {
                best_node = current;
                best_previous = previous;
            }
        }

        previous = current;
        current = current->next;
    }

    /*
     * Sacamos el nodo ganador de la cola.
     */
    if (best_previous == NULL) {
        /*
         * El mejor era el primero.
         */
        queue->front = best_node->next;
    } else {
        /*
         * El mejor estaba en medio o al final.
         */
        best_previous->next = best_node->next;
    }

    /*
     * Si el mejor era el ultimo, actualizamos rear.
     */
    if (queue->rear == best_node) {
        queue->rear = best_previous;
    }

    *ship_out = best_node->ship;

    free(best_node);
    queue->size--;

    /*
     * Si la cola quedo vacia, rear tambien debe quedar en NULL.
     */
    if (queue->size == 0) {
        queue->front = NULL;
        queue->rear = NULL;
    }

    return 1;
}

/*
 * Ejecuta un paso del calendarizador STRN.
 *
 * A diferencia de SJF, STRN no deja necesariamente que el barco
 * termine completo. Ejecuta una unidad y luego reevalua la cola.
 */
void strnFreeRTOSStep(ReadyQueue *queue) {
    if (queue == NULL) {
        return;
    }

    if (isQueueEmpty(queue)) {
        printf("%s vacia. No hay barcos para calendarizar.\n", queue->name);
        return;
    }

    ShipTask *current_ship = NULL;

    /*
     * Sacamos de la cola el barco con menor remaining_time.
     */
    if (!dequeueShortestRemainingTime(queue, &current_ship)) {
        return;
    }

    if (current_ship == NULL) {
        return;
    }

    printf("\n[STRN] Turno para %s desde la %s\n",
           current_ship->name,
           queue->name);

    printf("[STRN] %s tiene remaining_time %d\n",
           current_ship->name,
           current_ship->remaining_time);

    /*
     * STRN ejecuta una sola unidad.
     * Despues vuelve a revisar quien tiene menor tiempo restante.
     */
    if (!isShipFinished(current_ship)) {
        printf("[STRN] Despertando %s | restante antes: %d\n",
               current_ship->name,
               current_ship->remaining_time);

        /*
         * Guardamos el handle del scheduler actual dentro del barco.
         *
         * Asi, cuando el barco termine su unidad de ejecucion,
         * puede avisarle de vuelta al scheduler.
         */
        setShipSchedulerHandle(
            current_ship,
            xTaskGetCurrentTaskHandle()
        );

        /*
         * Aqui el calendarizador activa la task real del barco.
         */
        wakeShipTask(current_ship);

        /*
         * Esperamos a que la task del barco avance una unidad.
         * Esta espera ahora es bloqueante, no por polling.
         */
        waitForShipExecution(current_ship);
    }

    /*
     * Si no termino, vuelve a la cola.
     * Luego STRN volvera a escoger el menor remaining_time.
     */
    if (!isShipFinished(current_ship)) {
        printf("[STRN] %s no termino. Vuelve a la cola con restante %d.\n",
               current_ship->name,
               current_ship->remaining_time);

        enqueue(queue, current_ship);
    } else {
        printf("[STRN] %s termino. Sale de la cola.\n",
               current_ship->name);
    }
}

/*
 * Ejecuta STRN sobre dos colas de listos.
 *
 * Por ahora hace:
 * - un paso STRN en la izquierda
 * - un paso STRN en la derecha
 *
 * Luego esto se puede conectar con Equidad, Letrero o Tico.
 */
void runSTRNFreeRTOSTwoQueues(
    ReadyQueue *left_queue,
    ReadyQueue *right_queue
) {
    int cycle = 1;

    printf("\n========== STRN CON TASKS REALES DE FREERTOS ==========\n");
    printf("Regla: menor remaining_time = corre primero\n");

    while (!isQueueEmpty(left_queue) || !isQueueEmpty(right_queue)) {
        printf("\n---------- Ciclo %d ----------\n", cycle);

        if (!isQueueEmpty(left_queue)) {
            strnFreeRTOSStep(left_queue);
        }

        if (!isQueueEmpty(right_queue)) {
            strnFreeRTOSStep(right_queue);
        }

        printQueue(left_queue);
        printQueue(right_queue);

        cycle++;

        /*
         * Delay pequeno para que la salida en consola sea legible.
         */
        vTaskDelay(pdMS_TO_TICKS(500));
    }

    printf("\n[STRN] Todas las colas quedaron vacias.\n");
}