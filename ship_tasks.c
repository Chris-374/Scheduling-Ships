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
         * La task queda esperando hasta que el scheduler la despierte.
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
    }

    ship->state = SHIP_FINISHED;

    printf("[TERMINADO] %s termino su recorrido.\n", ship->name);

    /*
     * Se elimina la task actual.
     * No se libera ship porque en este proyecto los barcos
     * estan creados como variables globales/static en main.c.
     */
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

    /*
     * Al crear el barco todavía no tiene una posición guardada
     * dentro del canal. Cuando RR/STRN lo saquen por cambio de
     * contexto, canal.c guardará estos campos.
     */
    ship->channel_has_position = 0;
    ship->channel_position = -1;
    ship->channel_direction = side;
    ship->channel_speed_counter = 0;

    ship->handle = NULL;

    BaseType_t result = xTaskCreatePinnedToCore(
        shipTaskFunction, // Funcion que ejecuta la task
        ship->name, // Nombre de la task
        SHIP_STACK_SIZE, // Stack en bytes en ESP-IDF
        ship, // Parametro que recibe la task
        1, // Prioridad FreeRTOS baja
        &ship->handle, // Aqui se guarda el handle
        tskNO_AFFINITY  // Puede correr en cualquier nucleo
    );

    if (result != pdPASS) {
        printf("[ERROR] No se pudo crear la task del barco %s\n", ship->name);
        return 0;
    }

    printf("[OK] Barco %s creado como task real de FreeRTOS.\n", ship->name);

    return 1;
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