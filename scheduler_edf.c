#include <stdio.h>
#include <stdlib.h>
#include "scheduler_edf.h"

/*
 * Inserta un barco en la cola usando EDF.
 *
 * EDF significa Earliest Deadline First.
 * En espanol: primero el deadline mas cercano.
 *
 * En este proyecto, el campo deadline representa el tiempo maximo
 * que el barco deberia durar en pasar el canal.
 *
 * Regla usada:
 * - Menor deadline = mayor urgencia.
 * - Si dos barcos tienen el mismo deadline, se respeta el orden de llegada.
 */
int enqueueByEDF(ReadyQueue *queue, Task task) {
    if (queue == NULL) {
        return 0;
    }

    if (queue->size >= queue->capacity) {
        printf("[ERROR] La %s esta llena. No se pudo agregar el barco %s.\n", queue->name, task.name);
        return 0;
    }

    Node *new_node = (Node *)malloc(sizeof(Node));
    if (new_node == NULL) {
        printf("[ERROR] No se pudo reservar memoria para el barco %s.\n", task.name);
        return 0;
    }

    new_node->task = task;
    new_node->next = NULL;

    if (queue->front == NULL) {
        queue->front = new_node;
        queue->rear = new_node;
        queue->size++;
        return 1;
    }

    if (task.deadline < queue->front->task.deadline) {
        new_node->next = queue->front;
        queue->front = new_node;
        queue->size++;
        return 1;
    }

    Node *current = queue->front;
    while (current->next != NULL && current->next->task.deadline <= task.deadline) {
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

/*
 * Ejecuta un paso del calendarizador EDF sobre una cola de listos.
 *
 * Como la cola ya se mantiene ordenada por deadline, el primer barco
 * siempre es el barco con el tiempo maximo mas cercano.
 */
void edfStep(ReadyQueue *queue) {
    if (queue == NULL) {
        return;
    }

    if (isQueueEmpty(queue)) {
        printf("%s vacia. No hay barcos para calendarizar.\n", queue->name);
        return;
    }

    Task current_task;
    if (!dequeue(queue, &current_task)) {
        return;
    }

    printf("EDF en %s: ejecutando %s con deadline=%d y tiempo=%d hasta terminar.\n",
           queue->name,
           current_task.name,
           current_task.deadline,
           current_task.remaining_time);

    if (current_task.remaining_time > current_task.deadline) {
        printf("[AVISO] %s necesita %d unidad(es), pero su deadline es %d. Podria incumplir tiempo real.\n",
               current_task.name,
               current_task.remaining_time,
               current_task.deadline);
    }

    current_task.remaining_time = 0;
    printf("%s termino y sale de la cola.\n", current_task.name);
}

/*
 * Ejecuta una simulacion EDF usando dos colas de listos:
 * una para el lado izquierdo y otra para el lado derecho.
 */
void runEDFTwoQueues(ReadyQueue *left_queue, ReadyQueue *right_queue) {
    int cycle = 1;

    printf("\n========== SIMULACION EDF CON DOS COLAS ==========");
    printf("\nRegla: menor deadline = mayor urgencia de atencion.\n");
    printf("El deadline representa el tiempo maximo permitido para pasar el canal.\n");

    while (!isQueueEmpty(left_queue) || !isQueueEmpty(right_queue)) {
        printf("\n---------- Ciclo %d ----------\n", cycle);

        if (!isQueueEmpty(left_queue)) {
            edfStep(left_queue);
        }

        if (!isQueueEmpty(right_queue)) {
            edfStep(right_queue);
        }

        printQueue(left_queue);
        printQueue(right_queue);

        cycle++;
    }

    printf("\nTodas las colas quedaron vacias.\n");
}
