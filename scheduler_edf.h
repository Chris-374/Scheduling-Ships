#ifndef SCHEDULER_EDF_H
#define SCHEDULER_EDF_H

#include "scheduler.h"

int enqueueByEDF(ReadyQueue *queue, Task task);
void edfStep(ReadyQueue *queue);
void runEDFTwoQueues(ReadyQueue *left_queue, ReadyQueue *right_queue);

#endif
