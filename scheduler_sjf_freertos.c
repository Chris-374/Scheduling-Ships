#include <stdio.h>
#include <stdlib.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "scheduler_sjf_freertos.h"
#include "ship_tasks.h"

/*
 * Espera a que la task del barco termine una unidad de ejecucion.
 *
 * wakeShipTask() solamente despierta la task.
 * La task corre aparte, entonces el scheduler debe esperar un poco
 * a que el barco avance.
 */
static void waitForShipExecution(ShipTask *ship) {
    if (ship == NULL) {
        return;
    }

    /*
     * Esperamos a que la task pase de WAITING a RUNNING o FINISHED.
     * Esto evita que el scheduler siga demasiado rapido antes de que
     * la task realmente haya empezado a correr.
     */
    while (ship->state == SHIP_WAITING && !isShipFinished(ship)) {
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    /*
     * Mientras el barco este ejecutando, esperamos.
     */
    while (ship->state == SHIP_RUNNING) {
        vTaskDelay(pdMS_TO_TICKS(50));
    }

    /*
     * Pequena espera adicional para que los prints salgan mas ordenados.
     */
    vTaskDelay(pdMS_TO_TICKS(50));
}

/*
 * Busca y remueve de la cola el barco con menor burst_time.
 *
 * En esta implementacion:
 * - Menor burst_time = trabajo mas corto.
 *
 * Esto representa SJF no expropiativo.
 */
static int dequeueShortestJob(ReadyQueue *queue, ShipTask **ship_out) {
    if (queue == NULL || ship_out == NULL || queue->front == NULL) {
        return 0;
    }

    ReadyNode *current = queue->front;
    ReadyNode *previous = NULL;

    ReadyNode *best_node = queue->front;
    ReadyNode *best_previous = NULL;

    /*
     * Recorremos la cola buscando el barco con menor burst_time.
     */
    while (current != NULL) {
        if (current->ship != NULL && best_node->ship != NULL) {
            if (current->ship->burst_time < best_node->ship->burst_time) {
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
 * Ejecuta un paso del calendarizador SJF.
 *
 * A diferencia de Round Robin, aqui no usamos quantum.
 * El barco con menor burst_time corre hasta terminar.
 */
void sjfFreeRTOSStep(ReadyQueue *queue) {
    if (queue == NULL) {
        return;
    }

    if (isQueueEmpty(queue)) {
        printf("%s vacia. No hay barcos para calendarizar.\n", queue->name);
        return;
    }

    ShipTask *current_ship = NULL;

    /*
     * Sacamos de la cola el barco con menor burst_time.
     */
    if (!dequeueShortestJob(queue, &current_ship)) {
        return;
    }

    if (current_ship == NULL) {
        return;
    }

    printf("\n[SJF] Turno para %s desde la %s\n",
           current_ship->name,
           queue->name);

    printf("[SJF] %s tiene burst_time %d\n",
           current_ship->name,
           current_ship->burst_time);

    /*
     * En SJF no usamos quantum.
     * El barco elegido corre hasta terminar.
     */
    while (!isShipFinished(current_ship)) {
        printf("[SJF] Despertando %s | restante: %d\n",
               current_ship->name,
               current_ship->remaining_time);

        wakeShipTask(current_ship);

        waitForShipExecution(current_ship);
    }

    printf("[SJF] %s termino. Sale de la cola.\n",
           current_ship->name);
}

/*
 * Ejecuta SJF sobre dos colas de listos.
 *
 * Por ahora hace:
 * - un barco de la izquierda por SJF
 * - un barco de la derecha por SJF
 *
 * Luego esto se puede conectar con Equidad, Letrero o Tico.
 */
void runSJFFreeRTOSTwoQueues(
    ReadyQueue *left_queue,
    ReadyQueue *right_queue
) {
    int cycle = 1;

    printf("\n========== SJF CON TASKS REALES DE FREERTOS ==========\n");
    printf("Regla: menor burst_time = corre primero\n");

    while (!isQueueEmpty(left_queue) || !isQueueEmpty(right_queue)) {
        printf("\n---------- Ciclo %d ----------\n", cycle);

        if (!isQueueEmpty(left_queue)) {
            sjfFreeRTOSStep(left_queue);
        }

        if (!isQueueEmpty(right_queue)) {
            sjfFreeRTOSStep(right_queue);
        }

        printQueue(left_queue);
        printQueue(right_queue);

        cycle++;

        /*
         * Delay pequeno para que la salida en consola sea legible.
         */
        vTaskDelay(pdMS_TO_TICKS(500));
    }

    printf("\n[SJF] Todas las colas quedaron vacias.\n");
}