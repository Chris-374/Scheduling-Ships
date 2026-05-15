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

#define MAX_SHIPS_IN_CHANNEL 10
#define CHANNEL_TICK_MS 150

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

#define SHIP_REQUEST_QUEUE_SIZE 10

static QueueHandle_t ship_request_queue = NULL;
static int next_dynamic_ship_id = 100;
static int next_dynamic_deadline = 20;

/*
 * Evento de sensor de proximidad.
 *
 * Importante para defensa:
 * - La interrupcion/sensor NO ejecuta la logica pesada directamente.
 * - Solo deja un evento pendiente.
 * - El canal procesa ese evento en su task normal y ahi si baja agujas,
 *   evacua barcos y reordena colas.
 *
 * En esta version, la tecla P simula el sensor de proximidad.
 */
typedef enum {
    PROXIMITY_EVENT_APPROACH = 1
} ProximityEvent;

#define PROXIMITY_EVENT_QUEUE_SIZE 4
#define PROXIMITY_BLOCK_MS 3000

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

static int take_proximity_sensor_event(void) {
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

static const char *side_name(int side) {
    return (side == LEFT_SIDE) ? "Izquierda" : "Derecha";
}

static const char *direction_name(int direction) {
    return (direction == LEFT_SIDE)
        ? "Izquierda -> Derecha"
        : "Derecha -> Izquierda";
}

static int opposite_side(int side) {
    return (side == LEFT_SIDE) ? RIGHT_SIDE : LEFT_SIDE;
}

static ReadyQueue *queue_for_side(
    int side,
    ReadyQueue *left_queue,
    ReadyQueue *right_queue
) {
    return (side == LEFT_SIDE) ? left_queue : right_queue;
}

static int entry_position(int side) {
    return (side == LEFT_SIDE) ? 0 : CHANNEL_LENGTH - 1;
}

static int movement_step(int side) {
    return (side == LEFT_SIDE) ? 1 : -1;
}

static int exit_reached(int position, int direction) {
    if (direction == LEFT_SIDE) {
        return position >= CHANNEL_LENGTH;
    }

    return position < 0;
}

/*
 * Velocidad por tipo:
 * Patrulla avanza mas seguido.
 * Pesquera intermedio.
 * Normal mas lento.
 */
static int movement_period(ShipTask *ship) {
    if (ship == NULL) {
        return 3;
    }

    switch (ship->type) {
        case PATROL:
            return 1;

        case FISHING:
            return 2;

        case NORMAL:
        default:
            return 3;
    }
}

static int execute_ship_task_once(ShipTask *ship) {
    if (ship == NULL || isShipFinished(ship)) {
        return 0;
    }

    /*
     * Espera sincronizada con FreeRTOS.
     *
     * Antes el scheduler hacia polling revisando ship->state con while.
     * Ahora se bloquea esperando la notificacion que envia la task real
     * del barco cuando termina una unidad de ejecucion.
     */
    TaskHandle_t scheduler_handle = xTaskGetCurrentTaskHandle();

    setShipSchedulerHandle(ship, scheduler_handle);
    wakeShipTask(ship);

    uint32_t notified = ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(3000));

    setShipSchedulerHandle(ship, NULL);

    if (notified == 0) {
        printf("[WARN] Timeout esperando ejecucion de %s.\n", ship->name);
        return 0;
    }

    return 1;
}

static void init_channel(ChannelState *channel, int direction) {
    if (channel == NULL) {
        return;
    }

    channel->direction = direction;
    channel->length = CHANNEL_LENGTH;

    for (int i = 0; i < MAX_SHIPS_IN_CHANNEL; i++) {
        channel->ships[i].ship = NULL;
        channel->ships[i].position = -1;
        channel->ships[i].direction = direction;
        channel->ships[i].speed_counter = 0;
        channel->ships[i].ticks_used = 0;
        channel->ships[i].active = 0;
    }
}

static int channel_is_empty(ChannelState *channel) {
    if (channel == NULL) {
        return 1;
    }

    for (int i = 0; i < MAX_SHIPS_IN_CHANNEL; i++) {
        if (channel->ships[i].active) {
            return 0;
        }
    }

    return 1;
}

static int channel_count(ChannelState *channel) {
    int count = 0;

    if (channel == NULL) {
        return 0;
    }

    for (int i = 0; i < MAX_SHIPS_IN_CHANNEL; i++) {
        if (channel->ships[i].active) {
            count++;
        }
    }

    return count;
}

static int position_occupied(ChannelState *channel, int position, int ignore_index) {
    if (channel == NULL) {
        return 0;
    }

    for (int i = 0; i < MAX_SHIPS_IN_CHANNEL; i++) {
        if (i == ignore_index) {
            continue;
        }

        if (channel->ships[i].active &&
            channel->ships[i].position == position) {
            return 1;
        }
    }

    return 0;
}

static int find_free_slot(ChannelState *channel) {
    if (channel == NULL) {
        return -1;
    }

    for (int i = 0; i < MAX_SHIPS_IN_CHANNEL; i++) {
        if (!channel->ships[i].active) {
            return i;
        }
    }

    return -1;
}

static void get_short_name(ShipTask *ship, char *out, int size) {
    if (out == NULL || size <= 0) {
        return;
    }

    out[0] = '\0';

    if (ship == NULL) {
        return;
    }

    int i = 0;

    while (ship->name[i] != '\0' &&
           ship->name[i] != '_' &&
           i < size - 1) {
        out[i] = ship->name[i];
        i++;
    }

    out[i] = '\0';
}

static void print_channel(ChannelState *channel) {
    if (channel == NULL) {
        return;
    }

    printf("[CANAL] ");

    for (int pos = 0; pos < CHANNEL_LENGTH; pos++) {
        ShipTask *ship_here = NULL;

        for (int i = 0; i < MAX_SHIPS_IN_CHANNEL; i++) {
            if (channel->ships[i].active &&
                channel->ships[i].position == pos) {
                ship_here = channel->ships[i].ship;
                break;
            }
        }

        if (ship_here != NULL) {
            char short_name[8];
            get_short_name(ship_here, short_name, sizeof(short_name));
            printf("[%s]", short_name);
        } else {
            printf("[  ]");
        }
    }

    printf("\n");
}

static void update_hardware_channel(ChannelState *channel) {
    int positions[MAX_SHIPS_IN_CHANNEL];
    ShipType types[MAX_SHIPS_IN_CHANNEL];
    int count = 0;

    if (channel == NULL) {
        lcd_display_update_channel(NULL, NULL, 0, CHANNEL_LENGTH);
        return;
    }

    for (int i = 0; i < MAX_SHIPS_IN_CHANNEL; i++) {
        if (channel->ships[i].active && channel->ships[i].ship != NULL) {
            positions[count] = channel->ships[i].position;
            types[count] = channel->ships[i].ship->type;
            count++;
        }
    }

    lcd_display_update_channel(positions, types, count, CHANNEL_LENGTH);
}

static void reorder_queue_by_scheduler(ReadyQueue *queue, SchedulerType scheduler) {
    if (queue == NULL) {
        return;
    }

    if (scheduler == SCHEDULER_RR || scheduler == SCHEDULER_FCFS) {
        return;
    }

    ReadyNode *current = queue->front;
    queue->front = NULL;
    queue->rear = NULL;
    queue->size = 0;

    while (current != NULL) {
        ReadyNode *next = current->next;
        ShipTask *ship = current->ship;

        free(current);
        scheduler_enqueue_ordered(queue, ship, scheduler);

        current = next;
    }
}

static void process_pending_ship_requests(
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
        int burst_time = getDefaultBurstForType(request.type, CHANNEL_LENGTH);
        int priority = getDefaultPriorityForType(request.type);

        next_dynamic_deadline += getDefaultDeadlineForType(request.type, CHANNEL_LENGTH);
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

        ReadyQueue *target_queue = queue_for_side(
            request.side,
            left_queue,
            right_queue
        );

        if (!enqueue(target_queue, ship)) {
            printf("[ERROR] No se pudo insertar %s en la cola.\n", ship->name);
            continue;
        }

        reorder_queue_by_scheduler(target_queue, scheduler);

        printf("[NUEVO BARCO] %s agregado a %s. Se reorganiza la cola con %s.\n",
               ship->name,
               target_queue->name,
               scheduler_to_string(scheduler));

        printQueue(target_queue);
        lcd_display_update(left_queue, right_queue, NULL);
    }
}

static void clear_saved_channel_context(ShipTask *ship) {
    if (ship == NULL) {
        return;
    }

    ship->channel_has_position = 0;
    ship->channel_position = -1;
    ship->channel_direction = ship->side;
    ship->channel_speed_counter = 0;
}

static void save_channel_context(ShipInChannel *boat) {
    if (boat == NULL || boat->ship == NULL) {
        return;
    }

    boat->ship->channel_has_position = 1;
    boat->ship->channel_position = boat->position;
    boat->ship->channel_direction = boat->direction;
    boat->ship->channel_speed_counter = boat->speed_counter;
}

static void remove_from_channel(ChannelState *channel, int index) {
    if (channel == NULL || index < 0 || index >= MAX_SHIPS_IN_CHANNEL) {
        return;
    }

    channel->ships[index].ship = NULL;
    channel->ships[index].position = -1;
    channel->ships[index].direction = channel->direction;
    channel->ships[index].speed_counter = 0;
    channel->ships[index].ticks_used = 0;
    channel->ships[index].active = 0;
}

static void complete_ship_if_needed(ShipTask *ship) {
    if (ship == NULL) {
        return;
    }

    /*
     * Con el modelo actual, remaining_time se calcula a partir de
     * CHANNEL_LENGTH. Por eso el barco deberia terminar justo cuando
     * realiza el ultimo avance de salida.
     *
     * No se usa while para forzar la terminacion. Si algo queda
     * inconsistente, se reporta para depuracion.
     */
    if (!isShipFinished(ship)) {
        printf("[WARN] %s salio del canal pero aun tiene remaining_time=%d.\n",
               ship->name,
               ship->remaining_time);
    }
}

static void requeue_from_channel(
    ChannelState *channel,
    int index,
    ReadyQueue *left_queue,
    ReadyQueue *right_queue,
    SchedulerType scheduler
) {
    if (channel == NULL || index < 0 || index >= MAX_SHIPS_IN_CHANNEL) {
        return;
    }

    ShipInChannel *boat = &channel->ships[index];
    ShipTask *ship = boat->ship;

    if (ship == NULL) {
        remove_from_channel(channel, index);
        return;
    }

    save_channel_context(boat);

    ReadyQueue *return_queue = queue_for_side(
        boat->direction,
        left_queue,
        right_queue
    );

    printf("[CALENDARIZADOR] %s agotó su quantum. Guarda posicion %d y vuelve a la cola.\n",
           ship->name,
           boat->position);

    scheduler_enqueue_ordered(return_queue, ship, scheduler);
    remove_from_channel(channel, index);
}


static void emergency_requeue_from_channel(
    ChannelState *channel,
    int index,
    ReadyQueue *left_queue,
    ReadyQueue *right_queue,
    SchedulerType scheduler
) {
    if (channel == NULL || index < 0 || index >= MAX_SHIPS_IN_CHANNEL) {
        return;
    }

    ShipInChannel *boat = &channel->ships[index];
    ShipTask *ship = boat->ship;

    if (ship == NULL) {
        remove_from_channel(channel, index);
        return;
    }

    save_channel_context(boat);

    ReadyQueue *return_queue = queue_for_side(
        boat->direction,
        left_queue,
        right_queue
    );

    printf("[INTERRUPCION] %s sale temporalmente del canal y vuelve a cola en posicion %d.\n",
           ship->name,
           boat->position);

    scheduler_enqueue_ordered(return_queue, ship, scheduler);
    remove_from_channel(channel, index);
}

static void finish_from_channel(ChannelState *channel, int index) {
    if (channel == NULL || index < 0 || index >= MAX_SHIPS_IN_CHANNEL) {
        return;
    }

    ShipTask *ship = channel->ships[index].ship;

    if (ship != NULL) {
        complete_ship_if_needed(ship);
        clear_saved_channel_context(ship);

        printf("[CANAL] %s salió del canal por el extremo contrario.\n",
               ship->name);

        lcd_display_show_arrival(ship);
    }

    remove_from_channel(channel, index);
}

static int get_target_position_for_ship(ShipTask *ship, int side) {
    if (ship != NULL && ship->channel_has_position) {
        return ship->channel_position;
    }

    return entry_position(side);
}

static int get_target_direction_for_ship(ShipTask *ship, int side) {
    if (ship != NULL && ship->channel_has_position) {
        return ship->channel_direction;
    }

    return side;
}


static int queue_has_resumable_ship(ReadyQueue *queue) {
    if (queue == NULL) {
        return 0;
    }

    ReadyNode *current = queue->front;

    while (current != NULL) {
        if (current->ship != NULL && current->ship->channel_has_position) {
            return 1;
        }

        current = current->next;
    }

    return 0;
}

static ShipTask *select_next_equity_ship(
    ReadyQueue *queue,
    int can_start_new_ship
) {
    if (queue == NULL) {
        return NULL;
    }

    ReadyNode *current = queue->front;

    while (current != NULL) {
        ShipTask *ship = current->ship;

        if (ship != NULL) {
            if (ship->channel_has_position || can_start_new_ship) {
                return ship;
            }
        }

        current = current->next;
    }

    return NULL;
}

/*
 * Cuando un barco fue removido temporalmente del canal, conserva una
 * posicion guardada. En ese caso, el scheduler puede ordenar la cola,
 * pero no puede romper el orden fisico del canal.
 *
 * Ejemplo izquierda -> derecha:
 *   L1 en posicion 5 va adelante de L2 en posicion 4.
 *   Aunque RR/STRN/Priority pongan L2 primero en la cola, L2 no debe
 *   retomar antes que L1 si eso le permite rebasarlo.
 *
 * Por eso, antes de admitir un barco con posicion guardada, se busca si
 * existe otro barco del mismo sentido mas adelantado en la cola. Si existe,
 * ese barco fisicamente lider debe retomar primero.
 */
static int position_is_ahead(int direction, int candidate_position, int other_position) {
    if (direction == LEFT_SIDE) {
        return other_position > candidate_position;
    }

    return other_position < candidate_position;
}

static ShipTask *select_physical_leader_if_needed(
    ReadyQueue *queue,
    ShipTask *candidate
) {
    if (queue == NULL || candidate == NULL) {
        return candidate;
    }

    /*
     * Si hay barcos con posicion guardada, les damos prioridad de
     * recuperacion sobre barcos nuevos para reconstruir el canal sin
     * adelantamientos despues de una interrupcion o preempcion.
     */
    ShipTask *leader = NULL;
    ReadyNode *current = queue->front;

    while (current != NULL) {
        ShipTask *ship = current->ship;

        if (ship != NULL && ship->channel_has_position) {
            if (leader == NULL) {
                leader = ship;
            } else if (ship->channel_direction == leader->channel_direction &&
                       position_is_ahead(
                           leader->channel_direction,
                           leader->channel_position,
                           ship->channel_position
                       )) {
                leader = ship;
            }
        }

        current = current->next;
    }

    if (leader == NULL) {
        return candidate;
    }

    if (!candidate->channel_has_position) {
        return leader;
    }

    if (leader->channel_direction == candidate->channel_direction &&
        position_is_ahead(
            candidate->channel_direction,
            candidate->channel_position,
            leader->channel_position
        )) {
        return leader;
    }

    return candidate;
}

static int admit_one_ship_equity(
    ChannelState *channel,
    ReadyQueue *left_queue,
    ReadyQueue *right_queue,
    int side,
    SchedulerType scheduler,
    int can_start_new_ship,
    int *started_new_ship
) {
    if (started_new_ship != NULL) {
        *started_new_ship = 0;
    }

    if (channel == NULL) {
        return 0;
    }

    if (channel_count(channel) >= MAX_SHIPS_IN_CHANNEL) {
        return 0;
    }

    ReadyQueue *queue = queue_for_side(side, left_queue, right_queue);

    if (isQueueEmpty(queue)) {
        return 0;
    }

    ShipTask *ship = select_next_equity_ship(queue, can_start_new_ship);
    ship = select_physical_leader_if_needed(queue, ship);

    if (ship == NULL) {
        return 0;
    }

    int was_already_in_batch = ship->channel_has_position;
    int target_position = get_target_position_for_ship(ship, side);
    int target_direction = get_target_direction_for_ship(ship, side);

    if (!channel_is_empty(channel) && channel->direction != target_direction) {
        return 0;
    }

    if (position_occupied(channel, target_position, -1)) {
        return 0;
    }

    int slot = find_free_slot(channel);

    if (slot < 0) {
        return 0;
    }

    if (!scheduler_remove_specific_ship(queue, ship)) {
        return 0;
    }

    channel->direction = target_direction;

    channel->ships[slot].ship = ship;
    channel->ships[slot].position = target_position;
    channel->ships[slot].direction = target_direction;
    channel->ships[slot].speed_counter = ship->channel_has_position
        ? ship->channel_speed_counter
        : 0;
    channel->ships[slot].ticks_used = 0;
    channel->ships[slot].active = 1;

    if (ship->channel_has_position) {
        printf("\n[CANAL] %s retoma el canal desde la posicion %d en sentido %s.\n",
               ship->name,
               target_position,
               direction_name(target_direction));
    } else {
        printf("\n[CANAL] %s entra al canal desde la %s en posicion %d.\n",
               ship->name,
               side_name(side),
               target_position);
    }

    if (!was_already_in_batch && started_new_ship != NULL) {
        *started_new_ship = 1;
    }

    lcd_display_update(left_queue, right_queue, ship);
    print_channel(channel);
    update_hardware_channel(channel);

    return 1;
}

static int admit_one_ship(
    ChannelState *channel,
    ReadyQueue *left_queue,
    ReadyQueue *right_queue,
    int side,
    SchedulerType scheduler
) {
    if (channel == NULL) {
        return 0;
    }

    if (channel_count(channel) >= MAX_SHIPS_IN_CHANNEL) {
        return 0;
    }

    ReadyQueue *queue = queue_for_side(side, left_queue, right_queue);

    if (isQueueEmpty(queue)) {
        return 0;
    }

    ShipTask *ship = scheduler_select_next_ship(queue, scheduler);
    ship = select_physical_leader_if_needed(queue, ship);

    if (ship == NULL) {
        return 0;
    }

    int target_position = get_target_position_for_ship(ship, side);
    int target_direction = get_target_direction_for_ship(ship, side);

    if (!channel_is_empty(channel) && channel->direction != target_direction) {
        return 0;
    }

    if (position_occupied(channel, target_position, -1)) {
        /*
         * La posicion donde debe entrar o retomar esta ocupada.
         * No esperamos con while. Simplemente no se admite en este tick.
         */
        return 0;
    }

    int slot = find_free_slot(channel);

    if (slot < 0) {
        return 0;
    }

    if (!scheduler_remove_specific_ship(queue, ship)) {
        return 0;
    }

    channel->direction = target_direction;

    channel->ships[slot].ship = ship;
    channel->ships[slot].position = target_position;
    channel->ships[slot].direction = target_direction;
    channel->ships[slot].speed_counter = ship->channel_has_position
        ? ship->channel_speed_counter
        : 0;
    channel->ships[slot].ticks_used = 0;
    channel->ships[slot].active = 1;

    if (ship->channel_has_position) {
        printf("\n[CANAL] %s retoma el canal desde la posicion %d en sentido %s.\n",
               ship->name,
               target_position,
               direction_name(target_direction));
    } else {
        printf("\n[CANAL] %s entra al canal desde la %s en posicion %d.\n",
               ship->name,
               side_name(side),
               target_position);
    }

    lcd_display_update(left_queue, right_queue, ship);
    print_channel(channel);
    update_hardware_channel(channel);

    return 1;
}

static void execute_ship_unit(ShipInChannel *boat) {
    if (boat == NULL || boat->ship == NULL) {
        return;
    }

    if (isShipFinished(boat->ship)) {
        return;
    }

    if (execute_ship_task_once(boat->ship)) {
        boat->ticks_used++;
    }
}

/*
 * Mueve todos los barcos que puedan moverse en este tick.
 *
 * Importante:
 * - Se recorre de adelante hacia atrás.
 * - No hay while esperando a que una posición se libere.
 * - Si la siguiente posición está libre, avanza.
 * - Si está ocupada, ese barco no avanza en este tick.
 * - Otros barcos igual se revisan.
 */
static int move_channel_tick(
    ChannelState *channel,
    ReadyQueue *left_queue,
    ReadyQueue *right_queue,
    int max_ticks,
    SchedulerType scheduler
) {
    if (channel == NULL || channel_is_empty(channel)) {
        return 0;
    }

    int completed_count = 0;

    int processed[MAX_SHIPS_IN_CHANNEL];

    for (int i = 0; i < MAX_SHIPS_IN_CHANNEL; i++) {
        processed[i] = 0;
    }

    for (int pass = 0; pass < MAX_SHIPS_IN_CHANNEL; pass++) {
        int selected = -1;

        for (int i = 0; i < MAX_SHIPS_IN_CHANNEL; i++) {
            if (!channel->ships[i].active || processed[i]) {
                continue;
            }

            if (selected == -1) {
                selected = i;
                continue;
            }

            if (channel->direction == LEFT_SIDE) {
                if (channel->ships[i].position > channel->ships[selected].position) {
                    selected = i;
                }
            } else {
                if (channel->ships[i].position < channel->ships[selected].position) {
                    selected = i;
                }
            }
        }

        if (selected == -1) {
            break;
        }

        processed[selected] = 1;

        ShipInChannel *boat = &channel->ships[selected];
        ShipTask *ship = boat->ship;

        if (ship == NULL) {
            remove_from_channel(channel, selected);
            continue;
        }

        boat->speed_counter++;

        if (boat->speed_counter < movement_period(ship)) {
            continue;
        }

        boat->speed_counter = 0;

        int next_position = boat->position + movement_step(boat->direction);

        if (exit_reached(next_position, boat->direction)) {
            printf("[CANAL] %s llegó al extremo de salida.\n", ship->name);

            /*
             * El ultimo avance, desde la ultima posicion visible hacia
             * el oceano de salida, tambien consume una unidad de ejecucion.
             * Asi remaining_time coincide con CHANNEL_LENGTH.
             */
            execute_ship_unit(boat);

            lcd_display_update(left_queue, right_queue, ship);
            finish_from_channel(channel, selected);
            completed_count++;
            continue;
        }

        if (position_occupied(channel, next_position, selected)) {
            printf("[CANAL] %s no avanza: posicion %d ocupada.\n",
                   ship->name,
                   next_position);
            continue;
        }

        printf("[CANAL] %s avanza de %d a %d.\n",
               ship->name,
               boat->position,
               next_position);

        boat->position = next_position;

        lcd_display_update(left_queue, right_queue, ship);

        /*
         * Se ejecuta una unidad del task real solo cuando el barco logró avanzar.
         */
        execute_ship_unit(boat);

        /*
         * Si el scheduler es expropiativo y agotó su quantum,
         * vuelve a la cola guardando su posición actual.
         */
        if (max_ticks > 0 &&
            boat->ticks_used >= max_ticks &&
            !isShipFinished(ship)) {
            requeue_from_channel(
                channel,
                selected,
                left_queue,
                right_queue,
                scheduler
            );
        }
    }

    print_channel(channel);
    update_hardware_channel(channel);

    return completed_count;
}

static void delay_channel_tick(void) {
    vTaskDelay(pdMS_TO_TICKS(CHANNEL_TICK_MS));
}

/*
 * Procesa el evento del sensor de proximidad fuera de la ISR.
 *
 * Esto modela la interrupcion pedida por el enunciado:
 * - Se bajan las agujas.
 * - Se detiene la admision temporalmente.
 * - Los barcos que estaban dentro del canal se sacan y vuelven a cola.
 * - Las colas se reordenan con el scheduler actual.
 * - Se espera un tiempo deterministico para simular el buque externo.
 */
static int handle_proximity_interrupt(
    ChannelState *channel,
    ReadyQueue *left_queue,
    ReadyQueue *right_queue,
    SchedulerType scheduler
) {
    if (!take_proximity_sensor_event()) {
        return 0;
    }

    printf("\n======================================================\n");
    printf("[INTERRUPCION] Sensor de proximidad detecto un buque.\n");
    printf("[INTERRUPCION] Bajando agujas y evacuando el canal.\n");
    printf("======================================================\n");

    lcd_display_set_gates(1, 1);

    for (int i = 0; i < MAX_SHIPS_IN_CHANNEL; i++) {
        if (channel->ships[i].active) {
            emergency_requeue_from_channel(
                channel,
                i,
                left_queue,
                right_queue,
                scheduler
            );
        }
    }

    reorder_queue_by_scheduler(left_queue, scheduler);
    reorder_queue_by_scheduler(right_queue, scheduler);

    print_channel(channel);
    update_hardware_channel(channel);
    lcd_display_update(left_queue, right_queue, NULL);

    printf("[INTERRUPCION] Buque externo pasando. Canal protegido.\n");
    vTaskDelay(pdMS_TO_TICKS(PROXIMITY_BLOCK_MS));

    printf("[INTERRUPCION] Buque externo paso. Levantando agujas y reanudando.\n");
    lcd_display_set_gates(0, 0);

    return 1;
}

/* =========================================
 * POLÍTICA 1: EQUIDAD
 * ========================================= */
static void run_equity(
    ReadyQueue *left_queue,
    ReadyQueue *right_queue,
    int W,
    int max_ticks,
    SchedulerType scheduler
) {
    int current_side = LEFT_SIDE;

    /*
     * completed_this_turn:
     *   barcos que ya cruzaron completamente en el turno actual.
     *
     * started_this_turn:
     *   barcos DISTINTOS autorizados a participar en este turno.
     *
     * Importante:
     *   W no representa quantum ni cantidad de admisiones repetidas.
     *   W representa barcos completos por sentido.
     *
     *   Con RR, un barco puede agotar quantum y volver a cola. Si ese barco
     *   ya habia sido autorizado para este turno, puede retomar aunque
     *   started_this_turn ya haya llegado a W.
     *
     *   Lo que NO se permite es iniciar barcos nuevos del mismo lado cuando
     *   ya se autorizaron W barcos para ese lote de Equidad.
     */
    int completed_this_turn = 0;
    int started_this_turn = 0;

    ChannelState channel;
    init_channel(&channel, current_side);

    printf("\n[CANAL] Iniciando control de flujo: EQUIDAD (W = %d)\n", W);
    lcd_display_set_direction(current_side);
    lcd_display_set_gates(0, 0);
    update_hardware_channel(&channel);

    while (!isQueueEmpty(left_queue) ||
           !isQueueEmpty(right_queue) ||
           !channel_is_empty(&channel)) {
        handle_proximity_interrupt(&channel, left_queue, right_queue, scheduler);

        process_pending_ship_requests(left_queue, right_queue, scheduler);

        ReadyQueue *active_queue = queue_for_side(
            current_side,
            left_queue,
            right_queue
        );

        ReadyQueue *other_queue = queue_for_side(
            opposite_side(current_side),
            left_queue,
            right_queue
        );

        if (channel_is_empty(&channel)) {
            int active_has_resumable = queue_has_resumable_ship(active_queue);
            int should_switch_by_w =
                completed_this_turn >= W && !active_has_resumable;
            int should_switch_by_empty_side =
                isQueueEmpty(active_queue) && !isQueueEmpty(other_queue);

            if (should_switch_by_w) {
                if (!isQueueEmpty(other_queue)) {
                    current_side = opposite_side(current_side);
                    channel.direction = current_side;
                    completed_this_turn = 0;
                    started_this_turn = 0;

                    lcd_display_set_direction(current_side);

                    printf("\n[CANAL] Equidad: ya pasaron %d barcos completos. ", W);
                    printf("Cambiando a la cola %s.\n", side_name(current_side));
                } else {
                    /*
                     * El otro lado no tiene barcos. Se reinicia el lote del
                     * mismo lado para garantizar flujo, tal como pide Equidad.
                     */
                    completed_this_turn = 0;
                    started_this_turn = 0;

                    printf("\n[CANAL] Equidad: ya pasaron %d barcos completos, ", W);
                    printf("pero el lado contrario esta vacio. Continua %s.\n",
                           side_name(current_side));
                }
            } else if (should_switch_by_empty_side) {
                current_side = opposite_side(current_side);
                channel.direction = current_side;
                completed_this_turn = 0;
                started_this_turn = 0;

                lcd_display_set_direction(current_side);

                printf("\n[CANAL] Equidad: no hay barcos en el lado activo. ");
                printf("Cambiando a la cola %s.\n", side_name(current_side));
            }
        }

        active_queue = queue_for_side(
            current_side,
            left_queue,
            right_queue
        );

        int can_start_new_ship = started_this_turn < W;
        int started_new_ship = 0;

        if (!isQueueEmpty(active_queue)) {
            if (admit_one_ship_equity(
                    &channel,
                    left_queue,
                    right_queue,
                    current_side,
                    scheduler,
                    can_start_new_ship,
                    &started_new_ship
                )) {
                if (started_new_ship) {
                    started_this_turn++;
                    printf("[CANAL] Equidad: barco autorizado del lado %s: %d/%d.\n",
                           side_name(current_side),
                           started_this_turn,
                           W);
                }
            }
        }

        int completed_now = move_channel_tick(
            &channel,
            left_queue,
            right_queue,
            max_ticks,
            scheduler
        );

        if (completed_now > 0) {
            completed_this_turn += completed_now;
            printf("[CANAL] Equidad: barcos completos del lado %s en este turno: %d/%d.\n",
                   side_name(current_side),
                   completed_this_turn,
                   W);
        }

        delay_channel_tick();
    }
}

/* =========================================
 * POLÍTICA 2: LETRERO
 * ========================================= */
static void run_sign(
    ReadyQueue *left_queue,
    ReadyQueue *right_queue,
    int sign_duration,
    int max_ticks,
    SchedulerType scheduler
) {
    int current_side = LEFT_SIDE;
    int elapsed_time = 0;

    ChannelState channel;
    init_channel(&channel, current_side);

    printf("\n[CANAL] Iniciando control de flujo: LETRERO (Tiempo = %d ticks)\n",
           sign_duration);
    lcd_display_set_direction(current_side);
    lcd_display_set_gates(0, 0);
    update_hardware_channel(&channel);

    while (!isQueueEmpty(left_queue) ||
           !isQueueEmpty(right_queue) ||
           !channel_is_empty(&channel)) {
        handle_proximity_interrupt(&channel, left_queue, right_queue, scheduler);

        process_pending_ship_requests(left_queue, right_queue, scheduler);

        ReadyQueue *active_queue = queue_for_side(
            current_side,
            left_queue,
            right_queue
        );

        ReadyQueue *other_queue = queue_for_side(
            opposite_side(current_side),
            left_queue,
            right_queue
        );

        if (channel_is_empty(&channel)) {
            if (elapsed_time >= sign_duration ||
                (isQueueEmpty(active_queue) && !isQueueEmpty(other_queue))) {
                current_side = opposite_side(current_side);
                channel.direction = current_side;
                elapsed_time = 0;

                lcd_display_set_direction(current_side);

                printf("\n[CANAL] El letrero cambió. Nuevo sentido: %s.\n",
                       side_name(current_side));
            }
        }

        if (elapsed_time < sign_duration && !isQueueEmpty(active_queue)) {
            admit_one_ship(
                &channel,
                left_queue,
                right_queue,
                current_side,
                scheduler
            );
        }

        move_channel_tick(
            &channel,
            left_queue,
            right_queue,
            max_ticks,
            scheduler
        );

        elapsed_time++;
        delay_channel_tick();
    }
}

/* =========================================
 * POLÍTICA 3: TICO
 * ========================================= */
static void run_tico(
    ReadyQueue *left_queue,
    ReadyQueue *right_queue,
    int max_ticks,
    SchedulerType scheduler
) {
    int current_side = LEFT_SIDE;

    ChannelState channel;
    init_channel(&channel, current_side);

    printf("\n[CANAL] Iniciando control de flujo: TICO\n");
    printf("[CANAL] No hay control estricto, pero se evita choque por posiciones.\n");
    lcd_display_set_direction(current_side);
    lcd_display_set_gates(0, 0);
    update_hardware_channel(&channel);

    while (!isQueueEmpty(left_queue) ||
           !isQueueEmpty(right_queue) ||
           !channel_is_empty(&channel)) {
        handle_proximity_interrupt(&channel, left_queue, right_queue, scheduler);

        process_pending_ship_requests(left_queue, right_queue, scheduler);

        ReadyQueue *active_queue = queue_for_side(
            current_side,
            left_queue,
            right_queue
        );

        ReadyQueue *other_queue = queue_for_side(
            opposite_side(current_side),
            left_queue,
            right_queue
        );

        if (channel_is_empty(&channel) &&
            isQueueEmpty(active_queue) &&
            !isQueueEmpty(other_queue)) {
            current_side = opposite_side(current_side);
            channel.direction = current_side;

            lcd_display_set_direction(current_side);

            printf("\n[CANAL] TICO: cediendo paso al lado %s.\n",
                   side_name(current_side));
        }

        if (!isQueueEmpty(active_queue)) {
            admit_one_ship(
                &channel,
                left_queue,
                right_queue,
                current_side,
                scheduler
            );
        }

        move_channel_tick(
            &channel,
            left_queue,
            right_queue,
            max_ticks,
            scheduler
        );

        delay_channel_tick();
    }
}

/* =========================================
 * CONTROLADOR PRINCIPAL DEL CANAL
 * ========================================= */
void run_channel_flow(
    ChannelType channel_type,
    ReadyQueue *left_queue,
    ReadyQueue *right_queue,
    int param,
    int max_ticks,
    SchedulerType active_scheduler
) {
    if (param <= 0) {
        printf("[ERROR CANAL] El parametro de configuracion debe ser mayor que 0.\n");
        return;
    }

    switch (channel_type) {
        case CHANNEL_EQUITY:
            run_equity(
                left_queue,
                right_queue,
                param,
                max_ticks,
                active_scheduler
            );
            break;

        case CHANNEL_SIGN:
            run_sign(
                left_queue,
                right_queue,
                param,
                max_ticks,
                active_scheduler
            );
            break;

        case CHANNEL_TICO:
            run_tico(
                left_queue,
                right_queue,
                max_ticks,
                active_scheduler
            );
            break;

        default:
            printf("\n[ERROR] Algoritmo de canal desconocido.\n");
            break;
    }

    lcd_display_update(left_queue, right_queue, NULL);
    lcd_display_update_channel(NULL, NULL, 0, CHANNEL_LENGTH);
    lcd_display_set_gates(0, 0);
    printf("\n[CANAL] Todas las colas estan vacias. El puente esta inactivo.\n");
}