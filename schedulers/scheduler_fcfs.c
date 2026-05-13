#include "scheduler_policy.h"

/*
 * FCFS:
 * - First Come First Served.
 * - Respeta orden de llegada.
 * - No es expropiativo.
 */
int scheduler_fcfs_goes_before(ShipTask *a, ShipTask *b) {
    (void)a;
    (void)b;
    return 0;
}
