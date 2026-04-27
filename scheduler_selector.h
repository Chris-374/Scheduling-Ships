#ifndef SCHEDULER_SELECTOR_H
#define SCHEDULER_SELECTOR_H

#include "scheduler.h"

typedef enum {
    SCHEDULER_RR = 1,
    SCHEDULER_PRIORITY = 2,
    SCHEDULER_SJF = 3,
    SCHEDULER_STRN = 4,
    SCHEDULER_FCFS = 5
} SchedulerType;

SchedulerType parseSchedulerType(const char *value);
const char *schedulerTypeToString(SchedulerType scheduler_type);
void insertTaskByScheduler(ReadyQueue *queue, Task task, SchedulerType scheduler_type);
void runSchedulerTwoQueues(SchedulerType scheduler_type,
                           ReadyQueue *left_queue,
                           ReadyQueue *right_queue,
                           int quantum);

#endif
