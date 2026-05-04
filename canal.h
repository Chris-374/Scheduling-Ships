#ifndef CANAL_H
#define CANAL_H

#include "ready_queue.h"

/* Declaración de la función de control de flujo por Equidad */
void run_channel_equity(ReadyQueue *left_queue, ReadyQueue *right_queue, int W);

#endif