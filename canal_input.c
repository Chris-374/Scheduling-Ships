#include "canal_internal.h"

static QueueHandle_t ship_request_queue = NULL;
static int next_dynamic_ship_id = 100;
static int next_dynamic_deadline = 20;
static QueueHandle_t proximity_event_queue = NULL;

static void ensure_proximity_event_queue(void) {
    if (proximity_event_queue != NULL) {
        return;
    }

    proximity_event_queue = xQueueCreate(
        PROXIMITY_EVENT_QUEUE_SIZE,
        sizeof(ProximityEvent)
    );

    if (proximity_event_queue == NULL) {
        printf("[ERROR] No se pudo crear la cola de eventos del sensor.\n");
    }
}

static void signal_proximity_sensor_event(void) {
    ensure_proximity_event_queue();

    if (proximity_event_queue == NULL) {
        return;
    }

    ProximityEvent event = PROXIMITY_EVENT_APPROACH;

    if (xQueueSend(proximity_event_queue, &event, 0) == pdPASS) {
        printf("[SENSOR] Evento de proximidad recibido.\n");
    } else {
        printf("[SENSOR] Cola de eventos llena. Se ignora alerta repetida.\n");
    }
}

int canal_take_proximity_sensor_event(void) {
    ensure_proximity_event_queue();

    if (proximity_event_queue == NULL) {
        return 0;
    }

    ProximityEvent event;
    return xQueueReceive(proximity_event_queue, &event, 0) == pdPASS;
}

static int key_to_ship_request(int key, ShipAddRequest *request) {
    if (request == NULL) {
        return 0;
    }

    switch (key) {
        case '1':
            request->side = LEFT_SIDE;
            request->type = NORMAL;
            return 1;
        case '2':
            request->side = LEFT_SIDE;
            request->type = FISHING;
            return 1;
        case '3':
            request->side = LEFT_SIDE;
            request->type = PATROL;
            return 1;
        case '4':
            request->side = RIGHT_SIDE;
            request->type = NORMAL;
            return 1;
        case '5':
            request->side = RIGHT_SIDE;
            request->type = FISHING;
            return 1;
        case '6':
            request->side = RIGHT_SIDE;
            request->type = PATROL;
            return 1;
        default:
            return 0;
    }
}

static void keyboard_input_task(void *pvParameters) {
    (void)pvParameters;

    printf("\n[TECLADO] Entrada dinamica habilitada.\n");
    printf("[TECLADO] 1 Normal-Izq | 2 Pesquera-Izq | 3 Patrulla-Izq\n");
    printf("[TECLADO] 4 Normal-Der | 5 Pesquera-Der | 6 Patrulla-Der\n");

    while (1) {
        int key = getchar();
        ShipAddRequest request;

        if (key == 'p' || key == 'P') {
            printf("\n[TECLADO] Sensor de proximidad simulado con tecla P.\n");
            signal_proximity_sensor_event();
            continue;
        }

        if (key_to_ship_request(key, &request)) {
            if (ship_request_queue != NULL) {
                if (xQueueSend(ship_request_queue, &request, 0) == pdPASS) {
                    printf("[TECLADO] Solicitud recibida: crear barco %s en %s.\n",
                           shipTypeToString(request.type),
                           sideToString(request.side));
                } else {
                    printf("[TECLADO] Cola de solicitudes llena. Intente de nuevo.\n");
                }
            }
        }

        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

void canal_start_keyboard_input(void) {
    ensure_proximity_event_queue();

    if (ship_request_queue == NULL) {
        ship_request_queue = xQueueCreate(
            SHIP_REQUEST_QUEUE_SIZE,
            sizeof(ShipAddRequest)
        );

        if (ship_request_queue == NULL) {
            printf("[ERROR] No se pudo crear la cola de solicitudes de teclado.\n");
            return;
        }
    }

    BaseType_t result = xTaskCreatePinnedToCore(
        keyboard_input_task,
        "KeyboardInputTask",
        4096,
        NULL,
        1,
        NULL,
        tskNO_AFFINITY
    );

    if (result != pdPASS) {
        printf("[ERROR] No se pudo crear la task de teclado.\n");
    }
}

void canal_process_pending_ship_requests(
    ReadyQueue *left_queue,
    ReadyQueue *right_queue,
    SchedulerType scheduler
) {
    if (ship_request_queue == NULL) {
        return;
    }

    ShipAddRequest request;

    while (xQueueReceive(ship_request_queue, &request, 0) == pdTRUE) {
        ShipTask *ship = (ShipTask *)malloc(sizeof(ShipTask));

        if (ship == NULL) {
            printf("[ERROR] No se pudo reservar memoria para el barco dinamico.\n");
            continue;
        }

        int ship_id = next_dynamic_ship_id++;
        int burst_time = getDefaultBurstForType(request.type, canal_get_channel_length());
        int priority = getDefaultPriorityForType(request.type);

        next_dynamic_deadline += getDefaultDeadlineForType(request.type, canal_get_channel_length());
        int deadline = next_dynamic_deadline;

        char name[NAME_SIZE];
        snprintf(
            name,
            sizeof(name),
            "%c%d_%s",
            request.side == LEFT_SIDE ? 'L' : 'R',
            ship_id,
            shipTypeToString(request.type)
        );

        if (!createShipTask(
                ship,
                ship_id,
                name,
                request.type,
                request.side,
                burst_time,
                priority,
                deadline
            )) {
            free(ship);
            continue;
        }

        ReadyQueue *target_queue = canal_queue_for_side(
            request.side,
            left_queue,
            right_queue
        );

        if (!enqueue(target_queue, ship)) {
            printf("[ERROR] No se pudo insertar %s en la cola.\n", ship->name);
            continue;
        }

        canal_reorder_queue_by_scheduler(target_queue, scheduler);

        printf("[NUEVO BARCO] %s agregado a %s. Se reorganiza la cola con %s.\n",
               ship->name,
               target_queue->name,
               scheduler_to_string(scheduler));

        printQueue(target_queue);
        lcd_display_update(left_queue, right_queue, NULL);
    }
}
