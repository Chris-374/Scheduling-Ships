#include "canal_internal.h"

void canal_run_equity(
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
    canal_init_channel(&channel, current_side);

    printf("\n[CANAL] Iniciando control de flujo: EQUIDAD (W = %d)\n", W);
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
            int active_has_resumable = canal_queue_has_resumable_ship(active_queue);
            int should_switch_by_w =
                completed_this_turn >= W && !active_has_resumable;
            int should_switch_by_empty_side =
                isQueueEmpty(active_queue) && !isQueueEmpty(other_queue);

            if (should_switch_by_w) {
                if (!isQueueEmpty(other_queue)) {
                    current_side = canal_opposite_side(current_side);
                    channel.direction = current_side;
                    completed_this_turn = 0;
                    started_this_turn = 0;

                    lcd_display_set_direction(current_side);

                    printf("\n[CANAL] Equidad: ya pasaron %d barcos completos. ", W);
                    printf("Cambiando a la cola %s.\n", canal_side_name(current_side));
                } else {
                    /*
                     * El otro lado no tiene barcos. Se reinicia el lote del
                     * mismo lado para garantizar flujo, tal como pide Equidad.
                     */
                    completed_this_turn = 0;
                    started_this_turn = 0;

                    printf("\n[CANAL] Equidad: ya pasaron %d barcos completos, ", W);
                    printf("pero el lado contrario esta vacio. Continua %s.\n",
                           canal_side_name(current_side));
                }
            } else if (should_switch_by_empty_side) {
                current_side = canal_opposite_side(current_side);
                channel.direction = current_side;
                completed_this_turn = 0;
                started_this_turn = 0;

                lcd_display_set_direction(current_side);

                printf("\n[CANAL] Equidad: no hay barcos en el lado activo. ");
                printf("Cambiando a la cola %s.\n", canal_side_name(current_side));
            }
        }

        active_queue = canal_queue_for_side(
            current_side,
            left_queue,
            right_queue
        );

        int can_start_new_ship = started_this_turn < W;
        int started_new_ship = 0;

        if (!isQueueEmpty(active_queue)) {
            if (canal_admit_one_ship_equity(
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
                           canal_side_name(current_side),
                           started_this_turn,
                           W);
                }
            }
        }

        int completed_now = canal_move_channel_tick(
            &channel,
            left_queue,
            right_queue,
            max_ticks,
            scheduler
        );

        if (completed_now > 0) {
            completed_this_turn += completed_now;
            printf("[CANAL] Equidad: barcos completos del lado %s en este turno: %d/%d.\n",
                   canal_side_name(current_side),
                   completed_this_turn,
                   W);
        }

        canal_delay_channel_tick();
    }
}

