#include <stdio.h>
#include <stdlib.h>

#include "scheduler_policy.h"

int scheduler_ship_goes_before(
    ShipTask *a,
    ShipTask *b,
    SchedulerType scheduler
) {
    if (a == NULL || b == NULL) {
        return 0;
    }

    switch (scheduler) {
        case SCHEDULER_RR:
            return scheduler_rr_goes_before(a, b);

        case SCHEDULER_FCFS:
            return scheduler_fcfs_goes_before(a, b);

        case SCHEDULER_PRIORITY:
            return scheduler_priority_goes_before(a, b);

        case SCHEDULER_SJF:
            return scheduler_sjf_goes_before(a, b);

        case SCHEDULER_STRN:
            return scheduler_strn_goes_before(a, b);

        case SCHEDULER_EDF:
            return scheduler_edf_goes_before(a, b);

        default:
            return 0;
    }
}

int scheduler_enqueue_ordered(
    ReadyQueue *queue,
    ShipTask *ship,
    SchedulerType scheduler
) {
    if (queue == NULL || ship == NULL) {
        return 0;
    }

    /* RR y FCFS respetan orden de llegada. */
    if (scheduler == SCHEDULER_RR || scheduler == SCHEDULER_FCFS) {
        return enqueue(queue, ship);
    }

    ReadyNode *new_node = (ReadyNode *)malloc(sizeof(ReadyNode));

    if (new_node == NULL) {
        printf("[ERROR] No se pudo reservar nodo para %s.\n", ship->name);
        return 0;
    }

    new_node->ship = ship;
    new_node->next = NULL;

    if (queue->front == NULL) {
        queue->front = new_node;
        queue->rear = new_node;
        queue->size++;
        return 1;
    }

    if (scheduler_ship_goes_before(ship, queue->front->ship, scheduler)) {
        new_node->next = queue->front;
        queue->front = new_node;
        queue->size++;
        return 1;
    }

    ReadyNode *current = queue->front;

    while (current->next != NULL &&
           !scheduler_ship_goes_before(ship, current->next->ship, scheduler)) {
        current = current->next;
    }

    new_node->next = current->next;
    current->next = new_node;

    if (new_node->next == NULL) {
        queue->rear = new_node;
    }

    queue->size++;

    return 1;
}

ShipTask *scheduler_select_next_ship(
    ReadyQueue *queue,
    SchedulerType scheduler
) {
    if (queue == NULL || queue->front == NULL) {
        return NULL;
    }

    /* RR y FCFS toman el primero que llego. */
    if (scheduler == SCHEDULER_RR || scheduler == SCHEDULER_FCFS) {
        return queue->front->ship;
    }

    ReadyNode *current = queue->front;
    ShipTask *best = current->ship;

    while (current != NULL) {
        if (scheduler_ship_goes_before(current->ship, best, scheduler)) {
            best = current->ship;
        }

        current = current->next;
    }

    return best;
}

int scheduler_remove_specific_ship(
    ReadyQueue *queue,
    ShipTask *ship
) {
    if (queue == NULL || ship == NULL || queue->front == NULL) {
        return 0;
    }

    ReadyNode *current = queue->front;
    ReadyNode *previous = NULL;

    while (current != NULL) {
        if (current->ship == ship) {
            if (previous == NULL) {
                queue->front = current->next;
            } else {
                previous->next = current->next;
            }

            if (queue->rear == current) {
                queue->rear = previous;
            }

            free(current);
            queue->size--;

            if (queue->size == 0) {
                queue->front = NULL;
                queue->rear = NULL;
            }

            return 1;
        }

        previous = current;
        current = current->next;
    }

    return 0;
}

const char *scheduler_to_string(SchedulerType scheduler) {
    switch (scheduler) {
        case SCHEDULER_RR:
            return "Round Robin";
        case SCHEDULER_PRIORITY:
            return "Prioridad";
        case SCHEDULER_SJF:
            return "SJF";
        case SCHEDULER_STRN:
            return "STRN";
        case SCHEDULER_FCFS:
            return "FCFS";
        case SCHEDULER_EDF:
            return "EDF";
        default:
            return "Desconocido";
    }
}
