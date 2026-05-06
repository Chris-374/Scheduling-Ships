#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "scheduler_rr_freertos.h"
#include "ship_tasks.h"

#include "lcd_display.h"

/*
 * Espera a que la task del barco termine su unidad de ejecucion.
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
 * Ejecuta un paso de Round Robin sobre una cola de barcos reales.
 */
void roundRobinFreeRTOSStep(
    ReadyQueue *queue,
    ReadyQueue *left_queue,
    ReadyQueue *right_queue,
    int quantum
) {
    if (queue == NULL) {
        return;
    }

    if (quantum <= 0) {
        printf("[ERROR] El quantum debe ser mayor que cero.\n");
        return;
    }

    if (isQueueEmpty(queue)) {
        printf("%s vacia. No hay barcos para calendarizar.\n", queue->name);
        return;
    }

    ShipTask *current_ship = NULL;

    /*
     * Round Robin toma el primer barco de la cola.
     */
    if (!dequeue(queue, &current_ship)) {
        return;
    }

    if (current_ship == NULL) {
        return;
    }

    lcd_display_update(left_queue, right_queue, current_ship);

    printf("\n[RR] Turno para %s desde la %s\n",
           current_ship->name,
           queue->name);

    /*
     * Ejecutamos al barco por quantum unidades como maximo.
     *
     * Cada unidad se logra despertando una vez la task real.
     */
    for (int i = 0; i < quantum; i++) {
        if (isShipFinished(current_ship)) {
            break;
        }

        printf("[RR] Despertando %s | unidad %d de %d\n",
               current_ship->name,
               i + 1,
               quantum);

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
     * Si el barco no termino, vuelve al final de la cola.
     * Esto es lo caracteristico de Round Robin.
     */
    if (!isShipFinished(current_ship)) {
        printf("[RR] %s no termino. Vuelve al final de la cola.\n",
               current_ship->name);

        enqueue(queue, current_ship);
    } else {
        printf("[RR] %s ya termino. Sale de la cola.\n",
               current_ship->name);
    }

    lcd_display_update(left_queue, right_queue, NULL);
}

/*
 * Ejecuta RR sobre dos colas de listos.
 *
 * Por ahora se ejecuta un paso sobre la cola izquierda y luego
 * un paso sobre la cola derecha.
 *
 * Despues, cuando se implemente el canal, esta funcion probablemente
 * cambiara para respetar el algoritmo de flujo:
 * - Equidad
 * - Letrero
 * - Tico
 */
void runRoundRobinFreeRTOSTwoQueues(
    ReadyQueue *left_queue,
    ReadyQueue *right_queue,
    int quantum
) {
    int cycle = 1;

    printf("\n========== RR CON TASKS REALES DE FREERTOS ==========\n");
    printf("Quantum: %d\n", quantum);

    lcd_display_update(left_queue, right_queue, NULL);

    while (!isQueueEmpty(left_queue) || !isQueueEmpty(right_queue)) {
        printf("\n---------- Ciclo %d ----------\n", cycle);

        if (!isQueueEmpty(left_queue)) {
            roundRobinFreeRTOSStep(
                left_queue,
                left_queue,
                right_queue,
                quantum
            );
        }

        if (!isQueueEmpty(right_queue)) {
            roundRobinFreeRTOSStep(
                right_queue,
                left_queue,
                right_queue,
                quantum
            );
        }

        printQueue(left_queue);
        printQueue(right_queue);

        cycle++;

        /*
         * Delay pequeno para que la salida en consola sea legible.
         */
        vTaskDelay(pdMS_TO_TICKS(500));
    }

    printf("\n[RR] Todas las colas quedaron vacias.\n");

    lcd_display_update(left_queue, right_queue, NULL);
}