#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "ship_tasks.h"

/*
 * Convierte el tipo de barco a texto.
 * Sirve para imprimir informacion en consola.
 */
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

/*
 * Convierte el lado del barco a texto.
 * Sirve para imprimir informacion en consola.
 */
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
 * Cada barco queda bloqueado esperando una notificacion.
 * El calendarizador despierta el barco usando wakeShipTask().
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
        /*
         * La task queda bloqueada aqui.
         * Esto NO consume CPU mientras espera.
         */
        ship->state = SHIP_WAITING;

        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        /*
         * Si mientras estaba esperando ya quedo como terminado,
         * no ejecutamos nada mas.
         */
        if (ship->remaining_time <= 0) {
            break;
        }

        ship->state = SHIP_RUNNING;

        printf("[EJECUTANDO] %s | restante antes: %d\n",
               ship->name,
               ship->remaining_time);

        /*
         * Simula una unidad de ejecucion.
         * En el futuro esto puede representar avance en LCD/LEDs/canal.
         */
        vTaskDelay(pdMS_TO_TICKS(500));

        ship->remaining_time--;

        printf("[PAUSA] %s | restante despues: %d\n",
               ship->name,
               ship->remaining_time);

        /*
         * Notificacion de regreso.
         *
         * El barco le avisa al scheduler:
         * "ya termine esta unidad de ejecucion".
         *
         * Asi el scheduler no necesita hacer polling revisando ship->state.
         */
        if (ship->scheduler_handle != NULL) {
            xTaskNotifyGive(ship->scheduler_handle);
        }
    }

    ship->state = SHIP_FINISHED;

    printf("[TERMINADO] %s termino su recorrido.\n", ship->name);

    /*
     * Si por alguna razon termina sin haber notificado antes,
     * avisamos al scheduler una ultima vez.
     */
    if (ship->scheduler_handle != NULL) {
        xTaskNotifyGive(ship->scheduler_handle);
    }

    vTaskDelete(NULL);
}

/*
 * Crea un barco y su task real de FreeRTOS.
 */
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

/*
 * Guarda el handle del scheduler dentro del barco.
 *
 * Esto permite que el barco notifique al scheduler cuando termina
 * una unidad de ejecucion.
 */
void setShipSchedulerHandle(ShipTask *ship, TaskHandle_t scheduler_handle) {
    if (ship == NULL) {
        return;
    }

    ship->scheduler_handle = scheduler_handle;
}

/*
 * Despierta la task real de un barco.
 * Esto lo usa el calendarizador.
 */
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

/*
 * Retorna 1 si el barco ya termino.
 * Retorna 0 si todavia puede ejecutarse.
 */
int isShipFinished(const ShipTask *ship) {
    if (ship == NULL) {
        return 1;
    }

    return ship->state == SHIP_FINISHED || ship->remaining_time <= 0;
}