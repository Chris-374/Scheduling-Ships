#ifndef APP_CONFIG_H
#define APP_CONFIG_H

#include "canal.h"
#include "ship_tasks.h"

#define CONFIG_MAX_SHIPS_PER_SIDE 16

/*
 * Configuracion central del proyecto.
 *
 * Este archivo evita tener que cambiar defines en main.c durante la defensa.
 * El archivo config.txt se embebe en el firmware por CMake y se interpreta al
 * arrancar el ESP32.
 */
typedef struct {
    SchedulerType scheduler;
    ChannelType channel_type;

    int channel_length;
    int boat_speed_ms;
    int visible_queue_count;
    int physical_led_count;

    int sign_change_time;
    int equity_w;
    int quantum;
    int proximity_block_ms;

    int enable_keyboard_input;

    ShipType initial_left[CONFIG_MAX_SHIPS_PER_SIDE];
    int initial_left_count;

    ShipType initial_right[CONFIG_MAX_SHIPS_PER_SIDE];
    int initial_right_count;
} AppConfig;

void app_config_load_defaults(AppConfig *config);
int app_config_load(AppConfig *config);
void app_config_print(const AppConfig *config);

int app_config_channel_param(const AppConfig *config);
int app_config_max_ticks(const AppConfig *config);

const char *app_config_scheduler_name(SchedulerType scheduler);
const char *app_config_channel_name(ChannelType channel_type);
const char *app_config_ship_type_name(ShipType type);

#endif
