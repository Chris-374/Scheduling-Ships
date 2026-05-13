#include "scheduler_policy.h"

/*
 * Prioridad:
 * - Menor numero significa mayor prioridad.
 */
int scheduler_priority_goes_before(ShipTask *a, ShipTask *b) {
    if (a == NULL || b == NULL) {
        return 0;
    }

    return a->priority < b->priority;
}
