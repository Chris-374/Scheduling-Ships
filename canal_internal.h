#ifndef CANAL_INTERNAL_H
#define CANAL_INTERNAL_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "canal.h"
#include "ship_tasks.h"
#include "ready_queue.h"
#include "lcd_display.h"
#include "schedulers/scheduler_policy.h"

#define MAX_SHIPS_IN_CHANNEL 32
#define SHIP_REQUEST_QUEUE_SIZE 10
#define PROXIMITY_EVENT_QUEUE_SIZE 4


typedef struct {
    ShipTask *ship;
    int position;
    int direction;
    int speed_counter;
    int ticks_used;
    int active;
} ShipInChannel;

typedef struct {
    ShipInChannel ships[MAX_SHIPS_IN_CHANNEL];
    int direction;
    int length;
} ChannelState;

typedef struct {
    ShipType type;
    Side side;
} ShipAddRequest;

typedef enum {
    PROXIMITY_EVENT_APPROACH = 1
} ProximityEvent;

/* Entrada dinamica / sensor simulado */
int canal_take_proximity_sensor_event(void);
void canal_process_pending_ship_requests(
    ReadyQueue *left_queue,
    ReadyQueue *right_queue,
    SchedulerType scheduler
);

/* Utilidades generales */
const char *canal_side_name(int side);
const char *canal_direction_name(int direction);
int canal_opposite_side(int side);
ReadyQueue *canal_queue_for_side(
    int side,
    ReadyQueue *left_queue,
    ReadyQueue *right_queue
);
int canal_entry_position(int side);
int canal_movement_step(int side);
int canal_exit_reached(int position, int direction);
int canal_movement_period(ShipTask *ship);
int canal_execute_ship_task_once(ShipTask *ship);

/* Estado del canal */
void canal_init_channel(ChannelState *channel, int direction);
int canal_channel_is_empty(ChannelState *channel);
int canal_channel_count(ChannelState *channel);
int canal_position_occupied(ChannelState *channel, int position, int ignore_index);
int canal_find_free_slot(ChannelState *channel);
void canal_print_channel(ChannelState *channel);
void canal_update_hardware_channel(ChannelState *channel);
void canal_reorder_queue_by_scheduler(ReadyQueue *queue, SchedulerType scheduler);
int canal_queue_has_resumable_ship(ReadyQueue *queue);

/* Admision, avance e interrupciones */
int canal_admit_one_ship_equity(
    ChannelState *channel,
    ReadyQueue *left_queue,
    ReadyQueue *right_queue,
    int side,
    SchedulerType scheduler,
    int can_start_new_ship,
    int *started_new_ship
);
int canal_admit_one_ship(
    ChannelState *channel,
    ReadyQueue *left_queue,
    ReadyQueue *right_queue,
    int side,
    SchedulerType scheduler
);
int canal_move_channel_tick(
    ChannelState *channel,
    ReadyQueue *left_queue,
    ReadyQueue *right_queue,
    int max_ticks,
    SchedulerType scheduler
);
void canal_delay_channel_tick(void);
int canal_handle_proximity_interrupt(
    ChannelState *channel,
    ReadyQueue *left_queue,
    ReadyQueue *right_queue,
    SchedulerType scheduler
);

/* Politicas de flujo */
void canal_run_equity(
    ReadyQueue *left_queue,
    ReadyQueue *right_queue,
    int W,
    int max_ticks,
    SchedulerType scheduler
);
void canal_run_sign(
    ReadyQueue *left_queue,
    ReadyQueue *right_queue,
    int sign_duration,
    int max_ticks,
    SchedulerType scheduler
);
void canal_run_tico(
    ReadyQueue *left_queue,
    ReadyQueue *right_queue,
    int max_ticks,
    SchedulerType scheduler
);

#endif
