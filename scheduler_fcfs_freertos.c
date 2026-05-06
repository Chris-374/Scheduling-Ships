#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "scheduler_fcfs_freertos.h"
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