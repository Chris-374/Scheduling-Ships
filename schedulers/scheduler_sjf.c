#include "scheduler_policy.h"

/*
 * SJF:
 * - Shortest Job First.
 * - Escoge el barco con menor tiempo total de ejecucion.
 */
int scheduler_sjf_goes_before(ShipTask *a, ShipTask *b) {
    if (a == NULL || b == NULL) {
        return 0;
    }

    return a->burst_time < b->burst_time;
}
