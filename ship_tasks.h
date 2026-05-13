#ifndef SHIP_TASKS_H
#define SHIP_TASKS_H

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

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

typedef struct {
    int id;                 // ID del barco/proceso
    char name[NAME_SIZE];   // Nombre del proceso/barco

    ShipType type;          // Tipo de barco
    Side side;              // Lado desde donde se genero el barco

    int burst_time;         // Tiempo total que necesita ejecutar
    int remaining_time;     // Tiempo restante de ejecucion

    int priority;           // Numero de prioridad
    int deadline;           // Deadline del proceso

    ShipState state;        // Estado del barco

    /*
     * Contexto fisico/logico dentro del canal.
     * Esto es lo que permite que RR/STRN retomen desde donde quedaron
     * cuando el barco vuelve a la cola por cambio de contexto.
     */
    int channel_has_position;
    int channel_position;
    int channel_direction;
    int channel_speed_counter;

    TaskHandle_t handle;            // Task real del barco en FreeRTOS
    TaskHandle_t scheduler_handle;  // Scheduler al que debe avisar al terminar una unidad
} ShipTask;

int createShipTask(
    ShipTask *ship,
    int id,
    const char *name,
    ShipType type,
    Side side,
    int burst_time,
    int priority,
    int deadline
);

void wakeShipTask(ShipTask *ship);


void setShipSchedulerHandle(ShipTask *ship, TaskHandle_t scheduler_handle);

int isShipFinished(const ShipTask *ship);

const char *shipTypeToString(ShipType type);
const char *sideToString(Side side);

/*
 * Valores por defecto calculados a partir del largo del canal.
 *
 * Modelo usado:
 *   remaining_time inicial = channel_length
 *
 * Esto significa que el tiempo restante representa cuantas unidades
 * de avance faltan para cruzar el canal completo. Las diferencias
 * entre tipos se reflejan en velocidad, prioridad y deadline.
 */
int getDefaultBurstForType(ShipType type, int channel_length);
int getDefaultDeadlineForType(ShipType type, int channel_length);
int getDefaultPriorityForType(ShipType type);

#endif