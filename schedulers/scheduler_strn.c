#include "scheduler_policy.h"

/*
 * STRN:
 * - Shortest Time Remaining Next.
 * - Escoge el barco con menor tiempo restante.
 * - En esta integracion es expropiativo porque main.c configura max_ticks = 1.
 */
int scheduler_strn_goes_before(ShipTask *a, ShipTask *b) {
    if (a == NULL || b == NULL) {
        return 0;
    }

    return a->remaining_time < b->remaining_time;
}
