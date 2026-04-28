#include <stdio.h>
#include <stdlib.h>
#include "scheduler_sjf.h"

int enqueueBySJF(ReadyQueue *queue, Task task) {
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

    /*
     * Caso 1: cola vacia.
     * El nuevo barco queda como primero y ultimo.
     */
    if (queue->front == NULL) {
        queue->front = new_node;
        queue->rear = new_node;
        queue->size++;
        return 1;
    }

    /*
     * Caso 2: el nuevo barco tiene menor tiempo que el primero actual.
     * En SJF, menor burst_time significa que se ejecuta primero.
     */
    if (task.burst_time < queue->front->task.burst_time) {
        new_node->next = queue->front;
        queue->front = new_node;
        queue->size++;
        return 1;
    }

    /*
     * Caso 3: buscar la posicion correcta.
     * Se avanza mientras el siguiente barco tenga tiempo menor o igual.
     * El <= permite respetar el orden de llegada cuando hay empate.
     */
    Node *current = queue->front;
    while (current->next != NULL && current->next->task.burst_time <= task.burst_time) {
        current = current->next;
    }

    new_node->next = current->next;
    current->next = new_node;

    /*
     * Si se inserto al final, hay que actualizar rear.
     */
    if (new_node->next == NULL) {
        queue->rear = new_node;
    }

    queue->size++;
    return 1;
}

void sjfStep(ReadyQueue *queue) {
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

    printf("SJF en %s: ejecutando %s con tiempo %d hasta terminar.\n",
           queue->name,
           current_task.name,
           current_task.burst_time);

    current_task.remaining_time = 0;
    printf("%s termino y sale de la cola.\n", current_task.name);
}

void runSJFTwoQueues(ReadyQueue *left_queue, ReadyQueue *right_queue) {
    int cycle = 1;

    printf("\n========== SIMULACION SJF CON DOS COLAS ==========");
    printf("\nRegla: menor tiempo de ejecucion = mayor prioridad de atencion.\n");

    while (!isQueueEmpty(left_queue) || !isQueueEmpty(right_queue)) {
        printf("\n---------- Ciclo %d ----------\n", cycle);

        /*
         * Por ahora se ejecuta un paso por cada lado.
         * Mas adelante, el algoritmo de flujo del canal decidira
         * si se atiende la cola izquierda o la derecha.
         */
        if (!isQueueEmpty(left_queue)) {
            sjfStep(left_queue);
        }

        if (!isQueueEmpty(right_queue)) {
            sjfStep(right_queue);
        }

        printQueue(left_queue);
        printQueue(right_queue);

        cycle++;
    }

    printf("\nTodas las colas quedaron vacias.\n");
}
