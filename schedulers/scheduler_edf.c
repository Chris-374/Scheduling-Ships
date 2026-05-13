#include "scheduler_policy.h"

/*
 * EDF:
 * - Earliest Deadline First.
 * - Escoge el barco con deadline mas cercano.
 */
int scheduler_edf_goes_before(ShipTask *a, ShipTask *b) {
    if (a == NULL || b == NULL) {
        return 0;
    }

    return a->deadline < b->deadline;
}
