#include <stdio.h>
#include <string.h>
#include "scheduler_selector.h"
#include "scheduler_rr.h"
#include "scheduler_priority.h"
#include "scheduler_sjf.h"
#include "scheduler_strn.h"
#include "scheduler_fcfs.h"
#include "scheduler_edf.h"

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

    if (strcmp(value, "strn") == 0 || strcmp(value, "STRN") == 0 || strcmp(value, "4") == 0) {
        return SCHEDULER_STRN;
    }

    if (strcmp(value, "fcfs") == 0 || strcmp(value, "FCFS") == 0 || strcmp(value, "5") == 0) {
        return SCHEDULER_FCFS;
    }

    if (strcmp(value, "edf") == 0 || strcmp(value, "EDF") == 0 || strcmp(value, "6") == 0) {
        return SCHEDULER_EDF;
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

void insertTaskByScheduler(ReadyQueue *queue, Task task, SchedulerType scheduler_type) {
    if (scheduler_type == SCHEDULER_PRIORITY) {
        enqueueByPriority(queue, task);
    } else if (scheduler_type == SCHEDULER_SJF) {
        enqueueBySJF(queue, task);
    } else if (scheduler_type == SCHEDULER_STRN) {
        enqueueBySTRN(queue, task);
    } else if (scheduler_type == SCHEDULER_EDF) {
        enqueueByEDF(queue, task);
    } else {
        /* RR y FCFS respetan el orden normal de llegada. */
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

        case SCHEDULER_STRN:
            runSTRNTwoQueues(left_queue, right_queue);
            break;

        case SCHEDULER_FCFS:
            runFCFSTwoQueues(left_queue, right_queue);
            break;

        case SCHEDULER_EDF:
            runEDFTwoQueues(left_queue, right_queue);
            break;

        default:
            printf("[ERROR] Calendarizador no valido.\n");
            break;
    }
}
