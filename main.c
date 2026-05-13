#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "app_config.h"
#include "canal.h"
#include "ship_tasks.h"
#include "ready_queue.h"
#include "scheduler_rr_freertos.h"
#include "scheduler_priority_freertos.h"
#include "scheduler_sjf_freertos.h"
#include "scheduler_strn_freertos.h"
#include "scheduler_fcfs_freertos.h"
#include "scheduler_edf_freertos.h"
#include "lcd_display.h"
#include "schedulers/scheduler_policy.h"

#define MAX_INITIAL_SHIPS_PER_SIDE 4

/*
 * Configuracion cargada desde config.txt.
 */
static AppConfig app_config;

/*
 * Barcos iniciales reales.
 *
 * Se dejan en memoria global para que sigan existiendo durante toda
 * la ejecucion del programa.
 */
static ShipTask left_ships[MAX_INITIAL_SHIPS_PER_SIDE];
static ShipTask right_ships[MAX_INITIAL_SHIPS_PER_SIDE];

/*
 * Colas de listos.
 */
static ReadyQueue left_queue;
static ReadyQueue right_queue;

static int min_int(int a, int b) {
    return (a < b) ? a : b;
}

static ShipType default_left_type(int index) {
    switch (index) {
        case 0:
            return NORMAL;
        case 1:
            return PATROL;
        case 2:
            return FISHING;
        default:
            return NORMAL;
    }
}

static ShipType default_right_type(int index) {
    switch (index) {
        case 0:
            return FISHING;
        case 1:
            return NORMAL;
        case 2:
            return PATROL;
        default:
            return NORMAL;
    }
}

static void create_and_enqueue_initial_ship(
    ShipTask *ship,
    ReadyQueue *queue,
    int id,
    const char *prefix,
    ShipType type,
    Side side
) {
    char name[NAME_SIZE];
    int channel_length = canal_get_channel_length();

    snprintf(name, sizeof(name), "%s%d_%s", prefix, id, shipTypeToString(type));

    if (!createShipTask(
            ship,
            id,
            name,
            type,
            side,
            getDefaultBurstForType(type, channel_length),
            getDefaultPriorityForType(type),
            getDefaultDeadlineForType(type, channel_length)
        )) {
        printf("[ERROR] No se pudo crear el barco inicial %s.\n", name);
        return;
    }

    if (!scheduler_enqueue_ordered(queue, ship, app_config.scheduler)) {
        printf("[ERROR] No se pudo insertar %s en %s.\n", name, queue->name);
    }
}

static void create_initial_ships_from_config(void) {
    int left_count = min_int(app_config.initial_left_ships, MAX_INITIAL_SHIPS_PER_SIDE);
    int right_count = min_int(app_config.initial_right_ships, MAX_INITIAL_SHIPS_PER_SIDE);

    left_count = min_int(left_count, app_config.ready_queue_ordered_count);
    right_count = min_int(right_count, app_config.ready_queue_ordered_count);

    for (int i = 0; i < left_count; i++) {
        create_and_enqueue_initial_ship(
            &left_ships[i],
            &left_queue,
            i + 1,
            "L",
            default_left_type(i),
            LEFT_SIDE
        );
    }

    for (int i = 0; i < right_count; i++) {
        create_and_enqueue_initial_ship(
            &right_ships[i],
            &right_queue,
            i + 1,
            "R",
            default_right_type(i),
            RIGHT_SIDE
        );
    }
}

/*
 * Esta es la task del calendarizador.
 *
 * Su trabajo es ejecutar el algoritmo seleccionado sobre las dos colas
 * y luego entregar las colas al controlador del canal.
 */
void schedulerTask(void *pvParameters) {
    (void)pvParameters;

    int max_ticks = get_max_ticks_from_config(&app_config);
    int channel_param = get_channel_param_from_config(&app_config);

    printf("\n[SCHEDULER] Iniciando calendarizador\n");
    printf("[SCHEDULER] Calendarizador seleccionado: %s\n",
           scheduler_to_string(app_config.scheduler));

    printQueue(&left_queue);
    printQueue(&right_queue);

    run_channel_flow(
        app_config.channel_type,
        &left_queue,
        &right_queue,
        channel_param,
        max_ticks,
        app_config.scheduler
    );

    printf("\n[SCHEDULER] Calendarizacion terminada.\n");

    destroyQueue(&left_queue);
    destroyQueue(&right_queue);

    vTaskDelete(NULL);
}

/*
 * Punto de entrada de ESP-IDF.
 */
void app_main(void) {
    printf("\n===== SCHEDULING SHIPS CON TASKS REALES =====\n");

    load_config_file(CONFIG_FILE_PATH, &app_config);

    canal_set_channel_length(app_config.channel_length);
    canal_set_tick_ms(app_config.boat_speed_ms);

    print_app_config(&app_config);

    initQueue(&left_queue, "Cola izquierda");
    initQueue(&right_queue, "Cola derecha");

    lcd_display_init();

    if (app_config.enable_keyboard_input) {
        canal_start_keyboard_input();
    }

    create_initial_ships_from_config();

    lcd_display_update(&left_queue, &right_queue, NULL);

    BaseType_t result = xTaskCreatePinnedToCore(
        schedulerTask,
        "SchedulerTask",
        4096,
        NULL,
        2,
        NULL,
        tskNO_AFFINITY
    );

    if (result != pdPASS) {
        printf("[ERROR] No se pudo crear la task del calendarizador.\n");
        return;
    }

    printf("[MAIN] Calendarizador creado correctamente.\n");
}
