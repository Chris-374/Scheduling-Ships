#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "ship_tasks.h"

#define NAME_SIZE 32
#define SHIP_STACK_SIZE 2048

typedef enum {
    LEFT_SIDE = 0,
    RIGHT_SIDE = 1
} Side;

typedef enum {
    NORMAL = 0,
    FISHING = 1,
    PATROL = 2
} ShipType;

typedef enum {
    SHIP_WAITING = 0,
    SHIP_RUNNING = 1,
    SHIP_FINISHED = 2
} ShipState;

/*
 * Esta estructura representa un barco del proyecto,
 * pero ahora tambien guarda el handle de la task real de FreeRTOS.
 */
typedef struct {
    int id;
    char name[NAME_SIZE];

    ShipType type;
    Side side;

    int burst_time;
    int remaining_time;

    int priority;
    int deadline;

    ShipState state;

    TaskHandle_t handle;
} ShipTask;

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
 * Cada barco queda esperando una notificacion.
 * Mas adelante, nuestro calendarizador va a llamar xTaskNotifyGive()
 * sobre el handle del barco que quiere ejecutar.
 */
void shipTaskFunction(void *pvParameters) {
    ShipTask *ship = (ShipTask *)pvParameters;

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
         * No avanza hasta que alguien le mande una notificacion.
         *
         * Esto nos sirve para que nuestro propio calendarizador
         * controle cuando corre cada barco.
         */
        ship->state = SHIP_WAITING;
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        /*
         * Cuando llega aqui, significa que el calendarizador
         * decidio ejecutar este barco por una unidad de tiempo.
         */
        ship->state = SHIP_RUNNING;

        printf("[EJECUTANDO] %s | restante antes: %d\n",
               ship->name,
               ship->remaining_time);

        /*
         * Simulamos una unidad de ejecucion.
         * Luego esto podria representar avance en LEDs, pantalla,
         * canal, etc.
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
     * Por ahora no hacemos free(ship) aqui porque los barcos
     * estan creados como variables globales en este ejemplo.
     *
     * Si despues los creamos con malloc, entonces si habria que liberar.
     */
    vTaskDelete(NULL);
}

/*
 * Crea un barco y su task real de FreeRTOS.
 *
 * Recibe un puntero a ShipTask ya existente.
 * La task creada queda asociada en ship->handle.
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

    BaseType_t result = xTaskCreatePinnedToCore(
        shipTaskFunction,       // Funcion que ejecuta la task
        ship->name,             // Nombre de la task
        SHIP_STACK_SIZE,        // Stack en bytes en ESP-IDF
        ship,                   // Parametro que recibe la task
        1,                      // Prioridad FreeRTOS baja
        &ship->handle,          // Aqui se guarda el handle
        tskNO_AFFINITY          // Puede correr en cualquier nucleo
    );

    if (result != pdPASS) {
        printf("[ERROR] No se pudo crear la task del barco %s\n", ship->name);
        return 0;
    }

    printf("[OK] Barco %s creado como task real de FreeRTOS.\n", ship->name);

    return 1;
}

/*
 * Esta funcion despierta una task de barco.
 * Luego nuestro calendarizador va a llamar algo parecido a esto.
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
 * Barcos de prueba.
 * Por ahora son globales para que sigan existiendo durante toda la ejecucion.
 */
ShipTask ship1;
ShipTask ship2;
ShipTask ship3;
ShipTask ship4;

void app_main(void) {
    printf("\n===== CREACION DE BARCOS COMO TASKS REALES =====\n");

    createShipTask(
        &ship1,
        1,
        "L1_Normal",
        NORMAL,
        LEFT_SIDE,
        5,
        4,
        12
    );

    createShipTask(
        &ship2,
        2,
        "L2_Patrulla",
        PATROL,
        LEFT_SIDE,
        3,
        1,
        5
    );

    createShipTask(
        &ship3,
        3,
        "R1_Pesquera",
        FISHING,
        RIGHT_SIDE,
        4,
        2,
        8
    );

    createShipTask(
        &ship4,
        4,
        "R2_Normal",
        NORMAL,
        RIGHT_SIDE,
        6,
        5,
        15
    );

    printf("\n===== TODAS LAS TASKS FUERON CREADAS =====\n");

    /*
     * Prueba manual temporal.
     * Esto solo demuestra que las tasks existen y se pueden despertar.
     *
     * Mas adelante, en vez de hacer esto manualmente,
     * lo hara nuestro calendarizador RR, Prioridad, SJF, etc.
     */

    while (1) {
        printf("\n[DEMO] Despertando L1_Normal\n");
        wakeShipTask(&ship1);
        vTaskDelay(pdMS_TO_TICKS(1000));

        printf("\n[DEMO] Despertando L2_Patrulla\n");
        wakeShipTask(&ship2);
        vTaskDelay(pdMS_TO_TICKS(1000));

        printf("\n[DEMO] Despertando R1_Pesquera\n");
        wakeShipTask(&ship3);
        vTaskDelay(pdMS_TO_TICKS(1000));

        printf("\n[DEMO] Despertando R2_Normal\n");
        wakeShipTask(&ship4);
        vTaskDelay(pdMS_TO_TICKS(1000));

        if (ship1.state == SHIP_FINISHED &&
            ship2.state == SHIP_FINISHED &&
            ship3.state == SHIP_FINISHED &&
            ship4.state == SHIP_FINISHED) {
            printf("\n[DEMO] Todos los barcos terminaron.\n");
            break;
        }
    }
}