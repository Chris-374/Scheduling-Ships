#include "scheduler_policy.h"

/*
 * Round Robin:
 * - Respeta orden de llegada.
 * - La expropiacion por quantum la maneja canal.c con max_ticks.
 */
int scheduler_rr_goes_before(ShipTask *a, ShipTask *b) {
    (void)a;
    (void)b;
    return 0;
}
