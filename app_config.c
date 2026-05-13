#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "app_config.h"

static void trim(char *text) {
    if (text == NULL) {
        return;
    }

    char *start = text;
    while (*start != '\0' && isspace((unsigned char)*start)) {
        start++;
    }

    if (start != text) {
        memmove(text, start, strlen(start) + 1);
    }

    int len = (int)strlen(text);
    while (len > 0 && isspace((unsigned char)text[len - 1])) {
        text[len - 1] = '\0';
        len--;
    }
}

static void to_lowercase(char *text) {
    if (text == NULL) {
        return;
    }

    for (int i = 0; text[i] != '\0'; i++) {
        text[i] = (char)tolower((unsigned char)text[i]);
    }
}

static int parse_scheduler(const char *value, SchedulerType *scheduler) {
    char temp[32];

    if (value == NULL || scheduler == NULL) {
        return 0;
    }

    snprintf(temp, sizeof(temp), "%s", value);
    trim(temp);
    to_lowercase(temp);

    if (strcmp(temp, "rr") == 0 || strcmp(temp, "round_robin") == 0) {
        *scheduler = SCHEDULER_RR;
        return 1;
    }

    if (strcmp(temp, "priority") == 0 || strcmp(temp, "prioridad") == 0) {
        *scheduler = SCHEDULER_PRIORITY;
        return 1;
    }

    if (strcmp(temp, "sjf") == 0) {
        *scheduler = SCHEDULER_SJF;
        return 1;
    }

    if (strcmp(temp, "strn") == 0 || strcmp(temp, "srtf") == 0) {
        *scheduler = SCHEDULER_STRN;
        return 1;
    }

    if (strcmp(temp, "fcfs") == 0) {
        *scheduler = SCHEDULER_FCFS;
        return 1;
    }

    if (strcmp(temp, "edf") == 0) {
        *scheduler = SCHEDULER_EDF;
        return 1;
    }

    return 0;
}

static int parse_channel_type(const char *value, ChannelType *channel_type) {
    char temp[32];

    if (value == NULL || channel_type == NULL) {
        return 0;
    }

    snprintf(temp, sizeof(temp), "%s", value);
    trim(temp);
    to_lowercase(temp);

    if (strcmp(temp, "equidad") == 0 || strcmp(temp, "equity") == 0) {
        *channel_type = CHANNEL_EQUITY;
        return 1;
    }

    if (strcmp(temp, "letrero") == 0 || strcmp(temp, "sign") == 0) {
        *channel_type = CHANNEL_SIGN;
        return 1;
    }

    if (strcmp(temp, "tico") == 0) {
        *channel_type = CHANNEL_TICO;
        return 1;
    }

    return 0;
}

static int clamp_int(int value, int min_value, int max_value) {
    if (value < min_value) {
        return min_value;
    }

    if (value > max_value) {
        return max_value;
    }

    return value;
}

static void apply_int_field(const char *key, const char *value, AppConfig *config) {
    int parsed = atoi(value);

    if (strcmp(key, "channel_length") == 0 || strcmp(key, "largo_canal") == 0) {
        config->channel_length = clamp_int(parsed, 1, 100);
    } else if (strcmp(key, "boat_speed_ms") == 0 || strcmp(key, "velocidad_barco") == 0) {
        config->boat_speed_ms = clamp_int(parsed, 20, 5000);
    } else if (strcmp(key, "ready_queue_ordered_count") == 0 || strcmp(key, "cantidad_ordenados") == 0) {
        config->ready_queue_ordered_count = clamp_int(parsed, 1, 4);
    } else if (strcmp(key, "sign_change_time") == 0 || strcmp(key, "tiempo_letrero") == 0) {
        config->sign_change_time = clamp_int(parsed, 1, 1000);
    } else if (strcmp(key, "equity_w") == 0 || strcmp(key, "w") == 0) {
        config->equity_w = clamp_int(parsed, 1, 1000);
    } else if (strcmp(key, "quantum") == 0) {
        config->quantum = clamp_int(parsed, 1, 1000);
    } else if (strcmp(key, "initial_left_ships") == 0 || strcmp(key, "barcos_iniciales_izquierda") == 0) {
        config->initial_left_ships = clamp_int(parsed, 0, 4);
    } else if (strcmp(key, "initial_right_ships") == 0 || strcmp(key, "barcos_iniciales_derecha") == 0) {
        config->initial_right_ships = clamp_int(parsed, 0, 4);
    } else if (strcmp(key, "enable_keyboard_input") == 0 || strcmp(key, "entrada_teclado") == 0) {
        config->enable_keyboard_input = parsed != 0;
    }
}

void load_default_config(AppConfig *config) {
    if (config == NULL) {
        return;
    }

    config->scheduler = SCHEDULER_RR;
    config->channel_type = CHANNEL_SIGN;

    config->channel_length = CHANNEL_LENGTH;
    config->boat_speed_ms = 150;
    config->ready_queue_ordered_count = 4;

    config->sign_change_time = 5;
    config->equity_w = 2;

    config->quantum = 2;

    config->initial_left_ships = 2;
    config->initial_right_ships = 2;
    config->enable_keyboard_input = 1;
}

int load_config_file(const char *path, AppConfig *config) {
    FILE *file;
    char line[128];

    if (path == NULL || config == NULL) {
        return 0;
    }

    load_default_config(config);

    file = fopen(path, "r");

    if (file == NULL) {
        printf("[CONFIG] No se encontro %s. Usando valores por defecto.\n", path);
        return 0;
    }

    while (fgets(line, sizeof(line), file) != NULL) {
        char *equals;
        char key[64];
        char value[64];

        trim(line);

        if (line[0] == '\0' || line[0] == '#') {
            continue;
        }

        equals = strchr(line, '=');

        if (equals == NULL) {
            printf("[CONFIG] Linea ignorada: %s\n", line);
            continue;
        }

        *equals = '\0';
        snprintf(key, sizeof(key), "%s", line);
        snprintf(value, sizeof(value), "%s", equals + 1);

        trim(key);
        trim(value);
        to_lowercase(key);

        if (strcmp(key, "scheduler") == 0 || strcmp(key, "calendarizador") == 0) {
            if (!parse_scheduler(value, &config->scheduler)) {
                printf("[CONFIG] Scheduler invalido: %s\n", value);
            }
        } else if (strcmp(key, "channel_type") == 0 || strcmp(key, "tipo_canal") == 0) {
            if (!parse_channel_type(value, &config->channel_type)) {
                printf("[CONFIG] Tipo de canal invalido: %s\n", value);
            }
        } else {
            apply_int_field(key, value, config);
        }
    }

    fclose(file);
    return 1;
}


static const char *config_scheduler_to_string(SchedulerType scheduler) {
    switch (scheduler) {
        case SCHEDULER_RR:
            return "RR";
        case SCHEDULER_PRIORITY:
            return "Prioridad";
        case SCHEDULER_SJF:
            return "SJF";
        case SCHEDULER_STRN:
            return "STRN";
        case SCHEDULER_FCFS:
            return "FCFS";
        case SCHEDULER_EDF:
            return "EDF";
        default:
            return "Desconocido";
    }
}

static const char *channel_type_to_string(ChannelType channel_type) {
    switch (channel_type) {
        case CHANNEL_EQUITY:
            return "Equidad";
        case CHANNEL_SIGN:
            return "Letrero";
        case CHANNEL_TICO:
            return "Tico";
        default:
            return "Desconocido";
    }
}

void print_app_config(const AppConfig *config) {
    if (config == NULL) {
        return;
    }

    printf("\n===== CONFIGURACION CARGADA =====\n");
    printf("Scheduler: %s\n", config_scheduler_to_string(config->scheduler));
    printf("Tipo de canal: %s\n", channel_type_to_string(config->channel_type));
    printf("Largo del canal: %d\n", config->channel_length);
    printf("Velocidad del canal/barco: %d ms por tick\n", config->boat_speed_ms);
    printf("Cantidad ordenada visible/lista: %d\n", config->ready_queue_ordered_count);
    printf("Tiempo letrero: %d ticks\n", config->sign_change_time);
    printf("Parametro W: %d\n", config->equity_w);
    printf("Quantum RR/STRN: %d\n", config->quantum);
    printf("Barcos iniciales izquierda: %d\n", config->initial_left_ships);
    printf("Barcos iniciales derecha: %d\n", config->initial_right_ships);
    printf("Entrada por teclado: %s\n", config->enable_keyboard_input ? "si" : "no");
    printf("=================================\n\n");
}

int get_channel_param_from_config(const AppConfig *config) {
    if (config == NULL) {
        return 1;
    }

    switch (config->channel_type) {
        case CHANNEL_EQUITY:
            return config->equity_w;

        case CHANNEL_SIGN:
            return config->sign_change_time;

        case CHANNEL_TICO:
        default:
            return 1;
    }
}

int get_max_ticks_from_config(const AppConfig *config) {
    if (config == NULL) {
        return 0;
    }

    if (config->scheduler == SCHEDULER_RR || config->scheduler == SCHEDULER_STRN) {
        return config->quantum;
    }

    return 0;
}
