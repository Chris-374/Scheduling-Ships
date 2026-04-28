#ifndef SCHEDULER_RR_H
#define SCHEDULER_RR_H

#include "scheduler.h"

void roundRobinStep(ReadyQueue *queue, int quantum);
void runRoundRobinTwoQueues(ReadyQueue *left_queue, ReadyQueue *right_queue, int quantum);

#endif
