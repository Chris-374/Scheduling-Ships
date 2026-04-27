#include <stdio.h>
#include <stdlib.h>
#include "scheduler_strn.h"

int enqueueBySTRN(ReadyQueue *queue, Task task) {
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
     * Caso 2: el nuevo barco tiene menor tiempo restante que el primero.
     * En STRN, menor remaining_time significa que debe ir antes.
     */
    if (task.remaining_time < queue->front->task.remaining_time) {
        new_node->next = queue->front;
        queue->front = new_node;
        queue->size++;
        return 1;
    }

    /*
     * Caso 3: buscar la posicion correcta dentro de la cola.
     * Se avanza mientras el siguiente barco tenga tiempo restante menor o igual.
     * El <= permite conservar el orden de llegada si dos barcos empatan.
     */
    Node *current = queue->front;
    while (current->next != NULL && current->next->task.remaining_time <= task.remaining_time) {
        current = current->next;
    }

    new_node->next = current->next;
    current->next = new_node;

    /*
     * Si el nodo se inserto al final, rear debe actualizarse.
     */
    if (new_node->next == NULL) {
        queue->rear = new_node;
    }

    queue->size++;
    return 1;
}

void strnStep(ReadyQueue *queue) {
    if (queue == NULL) {
        return;
    }

    if (isQueueEmpty(queue)) {
        printf("%s vacia. No hay barcos para calendarizar.\n", queue->name);
        return;
    }

    /*
     * Como la cola se mantiene ordenada por remaining_time,
     * el primer barco siempre es el de menor tiempo restante.
     */
    Task current_task;
    if (!dequeue(queue, &current_task)) {
        return;
    }

    /*
     * STRN es expropiativo/preemptive.
     * Para simular eso sin hilos reales, se ejecuta una unidad de tiempo.
     * Luego, si no termino, se vuelve a insertar ordenado por remaining_time.
     */
    int executed_time = 1;
    current_task.remaining_time -= executed_time;

    printf("STRN en %s: ejecutando %s por %d unidad. Restante: %d.\n",
           queue->name,
           current_task.name,
           executed_time,
           current_task.remaining_time);

    if (current_task.remaining_time > 0) {
        enqueueBySTRN(queue, current_task);
    } else {
        printf("%s termino y sale de la cola.\n", current_task.name);
    }
}

void runSTRNTwoQueues(ReadyQueue *left_queue, ReadyQueue *right_queue) {
    int cycle = 1;

    printf("\n========== SIMULACION STRN CON DOS COLAS ==========");
    printf("\nRegla: menor tiempo restante = mayor prioridad de atencion.\n");
    printf("STRN ejecuta 1 unidad por paso para simular expropiacion.\n");

    while (!isQueueEmpty(left_queue) || !isQueueEmpty(right_queue)) {
        printf("\n---------- Ciclo %d ----------\n", cycle);

        /*
         * Por ahora se ejecuta un paso por cada lado.
         * Mas adelante, el algoritmo de flujo del canal decidira
         * si se atiende la cola izquierda o la derecha.
         */
        if (!isQueueEmpty(left_queue)) {
            strnStep(left_queue);
        }

        if (!isQueueEmpty(right_queue)) {
            strnStep(right_queue);
        }

        printQueue(left_queue);
        printQueue(right_queue);

        cycle++;
    }

    printf("\nTodas las colas quedaron vacias.\n");
}
