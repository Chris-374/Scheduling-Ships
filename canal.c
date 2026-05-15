#include "canal_internal.h"

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
            canal_run_equity(
                left_queue,
                right_queue,
                param,
                max_ticks,
                active_scheduler
            );
            break;

        case CHANNEL_SIGN:
            canal_run_sign(
                left_queue,
                right_queue,
                param,
                max_ticks,
                active_scheduler
            );
            break;

        case CHANNEL_TICO:
            canal_run_tico(
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
    lcd_display_update_channel(NULL, NULL, 0, canal_get_channel_length());
    lcd_display_set_gates(0, 0);
    printf("\n[CANAL] Todas las colas estan vacias. El puente esta inactivo.\n");
}
