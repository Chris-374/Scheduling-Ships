#include "canal_internal.h"

void canal_run_sign(
    ReadyQueue *left_queue,
    ReadyQueue *right_queue,
    int sign_duration,
    int max_ticks,
    SchedulerType scheduler
) {
    int current_side = LEFT_SIDE;
    int elapsed_time = 0;

    ChannelState channel;
    canal_init_channel(&channel, current_side);

    printf("\n[CANAL] Iniciando control de flujo: LETRERO (Tiempo = %d ticks)\n",
           sign_duration);
    lcd_display_set_direction(current_side);
    lcd_display_set_gates(0, 0);
    canal_update_hardware_channel(&channel);

    while (!isQueueEmpty(left_queue) ||
           !isQueueEmpty(right_queue) ||
           !canal_channel_is_empty(&channel)) {
        canal_handle_proximity_interrupt(&channel, left_queue, right_queue, scheduler);

        canal_process_pending_ship_requests(left_queue, right_queue, scheduler);

        ReadyQueue *active_queue = canal_queue_for_side(
            current_side,
            left_queue,
            right_queue
        );

        ReadyQueue *other_queue = canal_queue_for_side(
            canal_opposite_side(current_side),
            left_queue,
            right_queue
        );

        if (canal_channel_is_empty(&channel)) {
            if (elapsed_time >= sign_duration ||
                (isQueueEmpty(active_queue) && !isQueueEmpty(other_queue))) {
                current_side = canal_opposite_side(current_side);
                channel.direction = current_side;
                elapsed_time = 0;

                lcd_display_set_direction(current_side);

                printf("\n[CANAL] El letrero cambió. Nuevo sentido: %s.\n",
                       canal_side_name(current_side));
            }
        }

        if (elapsed_time < sign_duration && !isQueueEmpty(active_queue)) {
            canal_admit_one_ship(
                &channel,
                left_queue,
                right_queue,
                current_side,
                scheduler
            );
        }

        canal_move_channel_tick(
            &channel,
            left_queue,
            right_queue,
            max_ticks,
            scheduler
        );

        elapsed_time++;
        canal_delay_channel_tick();
    }
}

