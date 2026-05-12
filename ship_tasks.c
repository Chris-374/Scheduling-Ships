#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "ship_tasks.h"

const char *shipTypeToString(ShipType type) {
    switch (type) {
        case NORMAL:
            return "Normal";
        case FISHING:
            return "Pesquera";
        case PATROL:
            return "Patrulla";
        default:
            return "Desconocido";
    }
}

const char *sideToString(Side side) {
    switch (side) {
        case LEFT_SIDE:
            return "Izquierda";
        case RIGHT_SIDE:
            return "Derecha";
        default:
            return "Desconocido";
    }
}

/*
 * Esta funcion es la que ejecuta cada task real de FreeRTOS.
 *
 * El barco se queda bloqueado esperando wakeShipTask().
 * Cuando ejecuta una unidad, reduce remaining_time.
 * Si existe scheduler_handle, avisa al scheduler con xTaskNotifyGive().
 *
 * Esto permite dos formas de espera:
 * - canal.c actual puede seguir esperando por estado/polling.
 * - schedulers optimizados pueden esperar con ulTaskNotifyTake().
 */
static void shipTaskFunction(void *pvParameters) {
    ShipTask *ship = (ShipTask *)pvParameters;

    if (ship == NULL) {
        vTaskDelete(NULL);
        return;
    }

    printf("\n[CREADA] Task real para barco %s\n", ship->name);
    printf("ID: %d | Tipo: %s | Lado: %s | Tiempo: %d | Prioridad: %d | Deadline: %d\n",
           ship->id,
           shipTypeToString(ship->type),
           sideToString(ship->side),
           ship->burst_time,
           ship->priority,
           ship->deadline);

    while (ship->remaining_time > 0) {
        ship->state = SHIP_WAITING;

        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        if (ship->remaining_time <= 0) {
            break;
        }

        ship->state = SHIP_RUNNING;

        printf("[EJECUTANDO] %s | restante antes: %d\n",
               ship->name,
               ship->remaining_time);

        vTaskDelay(pdMS_TO_TICKS(500));

        ship->remaining_time--;

        printf("[PAUSA] %s | restante despues: %d\n",
               ship->name,
               ship->remaining_time);

        /*
         * Notificacion opcional para schedulers optimizados.
         * No afecta al canal actual si scheduler_handle esta en NULL.
         */
        if (ship->scheduler_handle != NULL) {
            xTaskNotifyGive(ship->scheduler_handle);
        }
    }

    ship->state = SHIP_FINISHED;

    printf("[TERMINADO] %s termino su recorrido.\n", ship->name);

    vTaskDelete(NULL);
}

int createShipTask(
    ShipTask *ship,
    int id,
    const char *name,
    ShipType type,
    Side side,
    int burst_time,
    int priority,
    int deadline
) {
    if (ship == NULL || name == NULL) {
        return 0;
    }

    ship->id = id;

    strncpy(ship->name, name, NAME_SIZE - 1);
    ship->name[NAME_SIZE - 1] = '\0';

    ship->type = type;
    ship->side = side;

    ship->burst_time = burst_time;
    ship->remaining_time = burst_time;

    ship->priority = priority;
    ship->deadline = deadline;

    ship->state = SHIP_WAITING;

    /*
     * Contexto del canal.
     * No se debe quitar: canal.c lo usa para que RR/STRN continúen
     * desde la posicion donde quedaron.
     */
    ship->channel_has_position = 0;
    ship->channel_position = -1;
    ship->channel_direction = side;
    ship->channel_speed_counter = 0;

    ship->handle = NULL;
    ship->scheduler_handle = NULL;

    BaseType_t result = xTaskCreatePinnedToCore(
        shipTaskFunction,
        ship->name,
        SHIP_STACK_SIZE,
        ship,
        1,
        &ship->handle,
        tskNO_AFFINITY
    );

    if (result != pdPASS) {
        printf("[ERROR] No se pudo crear la task del barco %s\n", ship->name);
        return 0;
    }

    printf("[OK] Barco %s creado como task real de FreeRTOS.\n", ship->name);

    return 1;
}

void setShipSchedulerHandle(ShipTask *ship, TaskHandle_t scheduler_handle) {
    if (ship == NULL) {
        return;
    }

    ship->scheduler_handle = scheduler_handle;
}

void wakeShipTask(ShipTask *ship) {
    if (ship == NULL || ship->handle == NULL) {
        return;
    }

    if (ship->state == SHIP_FINISHED) {
        printf("[INFO] %s ya termino. No se puede despertar.\n", ship->name);
        return;
    }

    xTaskNotifyGive(ship->handle);
}

int isShipFinished(const ShipTask *ship) {
    if (ship == NULL) {
        return 1;
    }

    return ship->state == SHIP_FINISHED || ship->remaining_time <= 0;
}