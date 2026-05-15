#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "app_config.h"

/*
 * config.txt se agrega como EMBED_TXTFILES en CMake.
 * ESP-IDF genera estos simbolos automaticamente.
 */
extern const unsigned char _binary_config_txt_start[] asm("_binary_config_txt_start");
extern const unsigned char _binary_config_txt_end[] asm("_binary_config_txt_end");

static void copy_string(char *dst, size_t dst_size, const char *src)
{
    if (dst == NULL || dst_size == 0) {
        return;
    }

    if (src == NULL) {
        dst[0] = '\0';
        return;
    }

    strncpy(dst, src, dst_size - 1);
    dst[dst_size - 1] = '\0';
}

static int clamp_int(int value, int min_value, int max_value)
{
    if (value < min_value) {
        return min_value;
    }

    if (value > max_value) {
        return max_value;
    }

    return value;
}

static void trim(char *text)
{
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

static void to_lowercase(char *text)
{
    if (text == NULL) {
        return;
    }

    for (int i = 0; text[i] != '\0'; i++) {
        text[i] = (char)tolower((unsigned char)text[i]);
    }
}

static int parse_scheduler(const char *value, SchedulerType *scheduler)
{
    char temp[32];

    if (value == NULL || scheduler == NULL) {
        return 0;
    }

    copy_string(temp, sizeof(temp), value);
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

static int parse_channel_type(const char *value, ChannelType *channel_type)
{
    char temp[32];

    if (value == NULL || channel_type == NULL) {
        return 0;
    }

    copy_string(temp, sizeof(temp), value);
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

static int parse_ship_type(const char *value, ShipType *type)
{
    char temp[32];

    if (value == NULL || type == NULL) {
        return 0;
    }

    copy_string(temp, sizeof(temp), value);
    trim(temp);
    to_lowercase(temp);

    if (strcmp(temp, "normal") == 0 || strcmp(temp, "n") == 0) {
        *type = NORMAL;
        return 1;
    }

    if (strcmp(temp, "fishing") == 0 || strcmp(temp, "pesquera") == 0 ||
        strcmp(temp, "pesquero") == 0 || strcmp(temp, "p") == 0) {
        *type = FISHING;
        return 1;
    }

    if (strcmp(temp, "patrol") == 0 || strcmp(temp, "patrulla") == 0 ||
        strcmp(temp, "t") == 0) {
        *type = PATROL;
        return 1;
    }

    return 0;
}

static int parse_ship_list(const char *value, ShipType ships[], int *count)
{
    char temp[192];
    char *token;
    char *saveptr = NULL;
    int parsed_count = 0;

    if (value == NULL || ships == NULL || count == NULL) {
        return 0;
    }

    copy_string(temp, sizeof(temp), value);

    token = strtok_r(temp, ",", &saveptr);

    while (token != NULL && parsed_count < CONFIG_MAX_SHIPS_PER_SIDE) {
        ShipType type;

        trim(token);

        if (parse_ship_type(token, &type)) {
            ships[parsed_count] = type;
            parsed_count++;
        } else {
            printf("[CONFIG] Tipo de barco invalido en lista: %s\n", token);
        }

        token = strtok_r(NULL, ",", &saveptr);
    }

    *count = parsed_count;
    return 1;
}

void app_config_load_defaults(AppConfig *config)
{
    if (config == NULL) {
        return;
    }

    config->scheduler = SCHEDULER_RR;
    config->channel_type = CHANNEL_EQUITY;

    config->channel_length = CHANNEL_LENGTH_DEFAULT;
    config->boat_speed_ms = 150;
    config->visible_queue_count = 4;
    config->physical_led_count = 10;

    config->sign_change_time = 3;
    config->equity_w = 2;
    config->quantum = 2;
    config->proximity_block_ms = 3000;

    config->enable_keyboard_input = 1;

    config->initial_left_count = 4;
    config->initial_left[0] = NORMAL;
    config->initial_left[1] = PATROL;
    config->initial_left[2] = FISHING;
    config->initial_left[3] = NORMAL;

    config->initial_right_count = 4;
    config->initial_right[0] = FISHING;
    config->initial_right[1] = NORMAL;
    config->initial_right[2] = PATROL;
    config->initial_right[3] = FISHING;
}

static void apply_int_field(const char *key, const char *value, AppConfig *config)
{
    int parsed;

    if (key == NULL || value == NULL || config == NULL) {
        return;
    }

    parsed = atoi(value);

    if (strcmp(key, "channel_length") == 0 || strcmp(key, "largo_canal") == 0) {
        config->channel_length = clamp_int(parsed, 1, 32);
    } else if (strcmp(key, "boat_speed_ms") == 0 || strcmp(key, "velocidad_barco") == 0) {
        config->boat_speed_ms = clamp_int(parsed, 20, 5000);
    } else if (strcmp(key, "visible_queue_count") == 0 ||
               strcmp(key, "ready_queue_ordered_count") == 0 ||
               strcmp(key, "cantidad_ordenados") == 0) {
        config->visible_queue_count = clamp_int(parsed, 1, 4);
    } else if (strcmp(key, "physical_led_count") == 0 || strcmp(key, "leds_fisicos") == 0) {
        config->physical_led_count = clamp_int(parsed, 1, 32);
    } else if (strcmp(key, "sign_change_time") == 0 || strcmp(key, "tiempo_letrero") == 0) {
        config->sign_change_time = clamp_int(parsed, 1, 1000);
    } else if (strcmp(key, "equity_w") == 0 || strcmp(key, "w") == 0) {
        config->equity_w = clamp_int(parsed, 1, 1000);
    } else if (strcmp(key, "quantum") == 0 || strcmp(key, "rr_quantum") == 0) {
        config->quantum = clamp_int(parsed, 1, 1000);
    } else if (strcmp(key, "proximity_block_ms") == 0 || strcmp(key, "tiempo_interrupcion_ms") == 0) {
        config->proximity_block_ms = clamp_int(parsed, 100, 30000);
    } else if (strcmp(key, "enable_keyboard_input") == 0 || strcmp(key, "entrada_teclado") == 0) {
        config->enable_keyboard_input = parsed != 0;
    }
}

static void parse_config_text(const char *text, AppConfig *config)
{
    char line[256];
    int pos = 0;

    if (text == NULL || config == NULL) {
        return;
    }

    for (int i = 0; ; i++) {
        char c = text[i];

        if (c != '\n' && c != '\0' && pos < (int)sizeof(line) - 1) {
            line[pos++] = c;
        }

        if (c == '\n' || c == '\0') {
            char *equals;
            char key[96];
            char value[160];

            line[pos] = '\0';
            pos = 0;

            trim(line);

            if (line[0] == '\0' || line[0] == '#') {
                if (c == '\0') {
                    break;
                }
                continue;
            }

            equals = strchr(line, '=');

            if (equals == NULL) {
                printf("[CONFIG] Linea ignorada: %s\n", line);
                if (c == '\0') {
                    break;
                }
                continue;
            }

            *equals = '\0';
            copy_string(key, sizeof(key), line);
            copy_string(value, sizeof(value), equals + 1);

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
            } else if (strcmp(key, "initial_left") == 0 || strcmp(key, "initial_left_ships") == 0) {
                parse_ship_list(value, config->initial_left, &config->initial_left_count);
            } else if (strcmp(key, "initial_right") == 0 || strcmp(key, "initial_right_ships") == 0) {
                parse_ship_list(value, config->initial_right, &config->initial_right_count);
            } else {
                apply_int_field(key, value, config);
            }

            if (c == '\0') {
                break;
            }
        }
    }
}

int app_config_load(AppConfig *config)
{
    int len;
    char *buffer;

    if (config == NULL) {
        return 0;
    }

    app_config_load_defaults(config);

    len = (int)(_binary_config_txt_end - _binary_config_txt_start);

    if (len <= 0) {
        printf("[CONFIG] config.txt embebido esta vacio. Usando valores por defecto.\n");
        return 0;
    }

    buffer = (char *)malloc((size_t)len + 1);

    if (buffer == NULL) {
        printf("[CONFIG] No se pudo reservar memoria para config.txt.\n");
        return 0;
    }

    memcpy(buffer, _binary_config_txt_start, (size_t)len);
    buffer[len] = '\0';

    parse_config_text(buffer, config);

    free(buffer);
    return 1;
}

const char *app_config_scheduler_name(SchedulerType scheduler)
{
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

const char *app_config_channel_name(ChannelType channel_type)
{
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

const char *app_config_ship_type_name(ShipType type)
{
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

void app_config_print(const AppConfig *config)
{
    if (config == NULL) {
        return;
    }

    printf("\n===== CONFIGURACION CARGADA =====\n");
    printf("Scheduler: %s\n", app_config_scheduler_name(config->scheduler));
    printf("Tipo de canal: %s\n", app_config_channel_name(config->channel_type));
    printf("Largo del canal: %d\n", config->channel_length);
    printf("Velocidad/tick del canal: %d ms\n", config->boat_speed_ms);
    printf("Barcos visibles por cola: %d\n", config->visible_queue_count);
    printf("LEDs fisicos de canal: %d\n", config->physical_led_count);
    printf("Tiempo letrero: %d ticks\n", config->sign_change_time);
    printf("Parametro W: %d\n", config->equity_w);
    printf("Quantum RR/STRN: %d\n", config->quantum);
    printf("Interrupcion/sensor: %d ms\n", config->proximity_block_ms);
    printf("Entrada por teclado: %s\n", config->enable_keyboard_input ? "si" : "no");

    printf("Barcos iniciales izquierda (%d): ", config->initial_left_count);
    for (int i = 0; i < config->initial_left_count; i++) {
        printf("%s%s", app_config_ship_type_name(config->initial_left[i]),
               i + 1 < config->initial_left_count ? "," : "");
    }
    printf("\n");

    printf("Barcos iniciales derecha (%d): ", config->initial_right_count);
    for (int i = 0; i < config->initial_right_count; i++) {
        printf("%s%s", app_config_ship_type_name(config->initial_right[i]),
               i + 1 < config->initial_right_count ? "," : "");
    }
    printf("\n=================================\n\n");
}

int app_config_channel_param(const AppConfig *config)
{
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

int app_config_max_ticks(const AppConfig *config)
{
    if (config == NULL) {
        return 0;
    }

    if (config->scheduler == SCHEDULER_RR) {
        return config->quantum;
    }

    if (config->scheduler == SCHEDULER_STRN) {
        return 1;
    }

    return 0;
}