#ifndef APP_CONFIG_H
#define APP_CONFIG_H

#include "canal.h"

#define CONFIG_FILE_PATH "config.txt"

/*
 * Configuracion general del proyecto.
 *
 * Este struct centraliza los valores que antes estaban quemados con #define.
 */
typedef struct {
    SchedulerType scheduler;
    ChannelType channel_type;

    int channel_length;
    int boat_speed_ms;
    int ready_queue_ordered_count;

    int sign_change_time;
    int equity_w;

    int quantum;

    int initial_left_ships;
    int initial_right_ships;
    int enable_keyboard_input;
} AppConfig;

void load_default_config(AppConfig *config);
int load_config_file(const char *path, AppConfig *config);
void print_app_config(const AppConfig *config);
int get_channel_param_from_config(const AppConfig *config);
int get_max_ticks_from_config(const AppConfig *config);

#endif
