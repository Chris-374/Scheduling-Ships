#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "app_config.h"
#include "canal.h"
#include "ship_tasks.h"
#include "ready_queue.h"
#include "lcd_display.h"
#include "schedulers/scheduler_policy.h"

#define INITIAL_TASK_STACK_SIZE 4096

static AppConfig app_config;

static ShipTask left_ships[CONFIG_MAX_SHIPS_PER_SIDE];
static ShipTask right_ships[CONFIG_MAX_SHIPS_PER_SIDE];

static ReadyQueue left_queue;
static ReadyQueue right_queue;

static const char *side_prefix(Side side)
{
    return side == LEFT_SIDE ? "L" : "R";
}

static const char *ship_type_suffix(ShipType type)
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

static int create_initial_ship(
    ShipTask *ship,
    int id,
    int index,
    ShipType type,
    Side side
) {
    char name[NAME_SIZE];
    int burst_time;
    int priority;
    int deadline;

    if (ship == NULL) {
        return 0;
    }

    snprintf(
        name,
        sizeof(name),
        "%s%d_%s",
        side_prefix(side),
        index,
        ship_type_suffix(type)
    );

    burst_time = getDefaultBurstForType(type, app_config.channel_length);
    priority = getDefaultPriorityForType(type);
    deadline = getDefaultDeadlineForType(type, app_config.channel_length);

    return createShipTask(
        ship,
        id,
        name,
        type,
        side,
        burst_time,
        priority,
        deadline
    );
}

static void create_initial_ships_for_side(
    ShipTask ships[],
    ShipType types[],
    int count,
    Side side,
    ReadyQueue *queue,
    int *next_id
) {
    for (int i = 0; i < count && i < CONFIG_MAX_SHIPS_PER_SIDE; i++) {
        if (create_initial_ship(
                &ships[i],
                *next_id,
                i + 1,
                types[i],
                side
            )) {
            scheduler_enqueue_ordered(queue, &ships[i], app_config.scheduler);
            (*next_id)++;
        }
    }
}

static void schedulerTask(void *pvParameters)
{
    (void)pvParameters;

    int channel_param;
    int max_ticks;

    printf("\n[SCHEDULER] Iniciando calendarizador\n");

    printQueue(&left_queue);
    printQueue(&right_queue);

    printf("\n[SCHEDULER] Calendarizador seleccionado: %s\n",
           app_config_scheduler_name(app_config.scheduler));

    channel_param = app_config_channel_param(&app_config);
    max_ticks = app_config_max_ticks(&app_config);

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

void app_main(void)
{
    int next_id = 1;

    printf("\n===== SCHEDULING SHIPS CON TASKS REALES =====\n");

    app_config_load(&app_config);
    app_config_print(&app_config);

    canal_set_runtime_config(
        app_config.channel_length,
        app_config.boat_speed_ms,
        app_config.proximity_block_ms
    );

    lcd_display_set_config(
        app_config.physical_led_count,
        app_config.visible_queue_count
    );

    initQueue(&left_queue, "Cola izquierda");
    initQueue(&right_queue, "Cola derecha");

    lcd_display_init();

    if (app_config.enable_keyboard_input) {
        canal_start_keyboard_input();
    } else {
        printf("[TECLADO] Entrada dinamica deshabilitada por config.txt.\n");
    }

    create_initial_ships_for_side(
        left_ships,
        app_config.initial_left,
        app_config.initial_left_count,
        LEFT_SIDE,
        &left_queue,
        &next_id
    );

    create_initial_ships_for_side(
        right_ships,
        app_config.initial_right,
        app_config.initial_right_count,
        RIGHT_SIDE,
        &right_queue,
        &next_id
    );

    lcd_display_update(&left_queue, &right_queue, NULL);

    BaseType_t result = xTaskCreatePinnedToCore(
        schedulerTask,
        "SchedulerTask",
        INITIAL_TASK_STACK_SIZE,
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
