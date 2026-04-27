#include <stdio.h>
#include <string.h>
#include "scheduler_selector.h"
#include "scheduler_rr.h"
#include "scheduler_priority.h"
#include "scheduler_sjf.h"

SchedulerType parseSchedulerType(const char *value) {
    if (value == NULL) {
        return SCHEDULER_RR;
    }

    if (strcmp(value, "rr") == 0 || strcmp(value, "RR") == 0 || strcmp(value, "1") == 0) {
        return SCHEDULER_RR;
    }

    if (strcmp(value, "priority") == 0 || strcmp(value, "prioridad") == 0 ||
        strcmp(value, "PRIORITY") == 0 || strcmp(value, "2") == 0) {
        return SCHEDULER_PRIORITY;
    }

    if (strcmp(value, "sjf") == 0 || strcmp(value, "SJF") == 0 || strcmp(value, "3") == 0) {
        return SCHEDULER_SJF;
    }

    printf("[AVISO] Calendarizador '%s' no reconocido. Se usara RR por defecto.\n", value);
    return SCHEDULER_RR;
}

const char *schedulerTypeToString(SchedulerType scheduler_type) {
    switch (scheduler_type) {
        case SCHEDULER_RR:
            return "Round Robin";
        case SCHEDULER_PRIORITY:
            return "Prioridad";
        case SCHEDULER_SJF:
            return "SJF";
        default:
            return "Desconocido";
    }
}

void insertTaskByScheduler(ReadyQueue *queue, Task task, SchedulerType scheduler_type) {
    if (scheduler_type == SCHEDULER_PRIORITY) {
        enqueueByPriority(queue, task);
    } else if (scheduler_type == SCHEDULER_SJF) {
        enqueueBySJF(queue, task);
    } else {
        enqueue(queue, task);
    }
}

void runSchedulerTwoQueues(SchedulerType scheduler_type,
                           ReadyQueue *left_queue,
                           ReadyQueue *right_queue,
                           int quantum) {
    printf("\nCalendarizador seleccionado: %s\n", schedulerTypeToString(scheduler_type));

    switch (scheduler_type) {
        case SCHEDULER_RR:
            runRoundRobinTwoQueues(left_queue, right_queue, quantum);
            break;

        case SCHEDULER_PRIORITY:
            runPriorityTwoQueues(left_queue, right_queue);
            break;

        case SCHEDULER_SJF:
            runSJFTwoQueues(left_queue, right_queue);
            break;

        default:
            printf("[ERROR] Calendarizador no valido.\n");
            break;
    }
}
