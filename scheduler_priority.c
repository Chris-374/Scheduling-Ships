#include <stdio.h>
#include <stdlib.h>
#include "scheduler_priority.h"

int enqueueByPriority(ReadyQueue *queue, Task task) {
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
     * El nuevo nodo queda como primero y ultimo.
     */
    if (queue->front == NULL) {
        queue->front = new_node;
        queue->rear = new_node;
        queue->size++;
        return 1;
    }

    /*
     * Caso 2: el nuevo barco tiene mas prioridad que el primero actual.
     * En esta version, menor numero significa mayor prioridad.
     * Por ejemplo: prioridad 1 pasa antes que prioridad 3.
     */
    if (task.priority < queue->front->task.priority) {
        new_node->next = queue->front;
        queue->front = new_node;
        queue->size++;
        return 1;
    }

    /*
     * Caso 3: buscar la posicion correcta.
     * Se avanza mientras el siguiente nodo tenga prioridad menor o igual,
     * porque los numeros mas bajos representan mayor prioridad.
     * Esto hace que, en empates, se respete el orden de llegada.
     */
    Node *current = queue->front;
    while (current->next != NULL && current->next->task.priority <= task.priority) {
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

void priorityStep(ReadyQueue *queue) {
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

    printf("Prioridad en %s: ejecutando %s con prioridad %d hasta terminar.\n",
           queue->name,
           current_task.name,
           current_task.priority);

    current_task.remaining_time = 0;
    printf("%s termino y sale de la cola.\n", current_task.name);
}

void runPriorityTwoQueues(ReadyQueue *left_queue, ReadyQueue *right_queue) {
    int cycle = 1;

    printf("\n========== SIMULACION PRIORIDAD CON DOS COLAS ==========");
    printf("\nRegla: menor numero = mayor prioridad.\n");

    while (!isQueueEmpty(left_queue) || !isQueueEmpty(right_queue)) {
        printf("\n---------- Ciclo %d ----------\n", cycle);

        /*
         * Por ahora se ejecuta un paso por cada lado.
         * Mas adelante, el algoritmo de flujo del canal decidira
         * si se atiende la cola izquierda o la derecha.
         */
        if (!isQueueEmpty(left_queue)) {
            priorityStep(left_queue);
        }

        if (!isQueueEmpty(right_queue)) {
            priorityStep(right_queue);
        }

        printQueue(left_queue);
        printQueue(right_queue);

        cycle++;
    }

    printf("\nTodas las colas quedaron vacias.\n");
}
