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