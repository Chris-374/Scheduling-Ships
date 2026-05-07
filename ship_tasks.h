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
    int id; // ID del barco/proceso
    char name[NAME_SIZE]; //Nombre del proceso (string)

    ShipType type; //Tipo de barco
    Side side; //Lado del canal en el que esta el barco

    int burst_time; //Tiempo que le toma al barco terminar
    int remaining_time; // Tiempo restante

    int priority; //Num de prioridad
    int deadline; //Deadline del proceso 

    ShipState state; //Estado del barco: Espera, corriendo o terminado

    /*
     * Contexto fisico/logico dentro del canal.
     * Se usa cuando un scheduler expropiativo (RR/STRN)
     * saca el barco antes de que llegue al otro extremo.
     */
    int channel_has_position;
    int channel_position;
    int channel_direction;
    int channel_speed_counter;

    TaskHandle_t handle; // Puntero al task real en FreeRTOS
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

int isShipFinished(const ShipTask *ship);

const char *shipTypeToString(ShipType type);
const char *sideToString(Side side);

#endif