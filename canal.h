#ifndef CANAL_H
#define CANAL_H

#include "ready_queue.h"

/* 
 * W: Cantidad de barcos que pasan por lado.
 * max_ticks: Cuánto tiempo cruza un barco (0 = cruce completo, N = quantum de RR o STRN)
 */
void run_channel_equity(ReadyQueue *left_queue, ReadyQueue *right_queue, int W, int max_ticks);

#endif