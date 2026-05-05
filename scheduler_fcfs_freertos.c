#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "scheduler_fcfs_freertos.h"
#include "ship_tasks.h"

/*
 * Espera a que la task del barco termine una unidad de ejecucion.
 *
 * wakeShipTask() solamente despierta la task.
 * La task real corre aparte, entonces el scheduler debe esperar
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
 * Ejecuta un paso del calendarizador FCFS.
 *
 * FCFS toma el primer barco de la cola y lo ejecuta hasta terminar.
 */
void fcfsFreeRTOSStep(ReadyQueue *queue) {
    if (queue == NULL) {
        return;
    }

    if (isQueueEmpty(queue)) {
        printf("%s vacia. No hay barcos para calendarizar.\n", queue->name);
        return;
    }

    ShipTask *current_ship = NULL;

    /*
     * FCFS toma el primer barco que llego a la cola.
     */
    if (!dequeue(queue, &current_ship)) {
        return;
    }

    if (current_ship == NULL) {
        return;
    }

    printf("\n[FCFS] Turno para %s desde la %s\n",
           current_ship->name,
           queue->name);

    printf("[FCFS] %s fue el primero en la cola actual\n",
           current_ship->name);

    /*
     * En FCFS no usamos quantum.
     * El barco elegido corre hasta terminar.
     */
    while (!isShipFinished(current_ship)) {
        printf("[FCFS] Despertando %s | restante: %d\n",
               current_ship->name,
               current_ship->remaining_time);

        wakeShipTask(current_ship);

        waitForShipExecution(current_ship);
    }

    printf("[FCFS] %s termino. Sale de la cola.\n",
           current_ship->name);
}

/*
 * Ejecuta FCFS sobre dos colas de listos.
 *
 * Por ahora hace:
 * - un barco de la izquierda por FCFS
 * - un barco de la derecha por FCFS
 *
 * Luego esto se puede conectar con Equidad, Letrero o Tico.
 */
void runFCFSFreeRTOSTwoQueues(
    ReadyQueue *left_queue,
    ReadyQueue *right_queue
) {
    int cycle = 1;

    printf("\n========== FCFS CON TASKS REALES DE FREERTOS ==========\n");
    printf("Regla: primero en llegar = primero en ejecutarse\n");

    while (!isQueueEmpty(left_queue) || !isQueueEmpty(right_queue)) {
        printf("\n---------- Ciclo %d ----------\n", cycle);

        if (!isQueueEmpty(left_queue)) {
            fcfsFreeRTOSStep(left_queue);
        }

        if (!isQueueEmpty(right_queue)) {
            fcfsFreeRTOSStep(right_queue);
        }

        printQueue(left_queue);
        printQueue(right_queue);

        cycle++;

        /*
         * Delay pequeno para que la salida en consola sea legible.
         */
        vTaskDelay(pdMS_TO_TICKS(500));
    }

    printf("\n[FCFS] Todas las colas quedaron vacias.\n");
}