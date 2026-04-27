#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "scheduler.h"

Task createTask(int id, const char *name, ShipType type, Side side, int burst_time) {
    Task task;
    task.id = id;
    strncpy(task.name, name, NAME_SIZE - 1);
    task.name[NAME_SIZE - 1] = '\0';
    task.type = type;
    task.side = side;
    task.burst_time = burst_time;
    task.remaining_time = burst_time;

    task.priority = 0;
    task.deadline = 0;

    return task;
}

Task createTaskWithPriority(int id, const char *name, ShipType type, Side side, int burst_time, int priority) {
    Task task = createTask(id, name, type, side, burst_time);
    task.priority = priority;
    return task;
}

void setTaskPriority(Task *task, int priority) {
    if (task == NULL) {
        return;
    }

    task->priority = priority;
}

void initQueue(ReadyQueue *queue, const char *name, int capacity) {
    if (queue == NULL) {
        return;
    }

    queue->front = NULL;
    queue->rear = NULL;
    queue->size = 0;
    queue->capacity = capacity;

    strncpy(queue->name, name, NAME_SIZE - 1);
    queue->name[NAME_SIZE - 1] = '\0';
}

int isQueueEmpty(const ReadyQueue *queue) {
    return queue == NULL || queue->size == 0;
}

int enqueue(ReadyQueue *queue, Task task) {
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

    if (queue->rear == NULL) {
        queue->front = new_node;
        queue->rear = new_node;
    } else {
        queue->rear->next = new_node;
        queue->rear = new_node;
    }

    queue->size++;
    return 1;
}

int dequeue(ReadyQueue *queue, Task *task_out) {
    if (queue == NULL || task_out == NULL || queue->front == NULL) {
        return 0;
    }

    Node *temp = queue->front;
    *task_out = temp->task;

    queue->front = queue->front->next;

    if (queue->front == NULL) {
        queue->rear = NULL;
    }

    free(temp);
    queue->size--;
    return 1;
}

void printQueue(const ReadyQueue *queue) {
    if (queue == NULL) {
        return;
    }

    printf("\n%s [%d/%d]: ", queue->name, queue->size, queue->capacity);

    if (queue->front == NULL) {
        printf("vacia\n");
        return;
    }

    Node *current = queue->front;
    while (current != NULL) {
        printf("%s(id=%d, tipo=%s, restante=%d, prioridad=%d)",
               current->task.name,
               current->task.id,
               shipTypeToString(current->task.type),
               current->task.remaining_time,
               current->task.priority);

        if (current->next != NULL) {
            printf(" -> ");
        }

        current = current->next;
    }

    printf("\n");
}

void destroyQueue(ReadyQueue *queue) {
    if (queue == NULL) {
        return;
    }

    Task discarded;
    while (dequeue(queue, &discarded)) {
        /* Se libera cada nodo mediante dequeue. */
    }
}

const char *shipTypeToString(ShipType type) {
    switch (type) {
        case NORMAL:
            return "Normal";
        case FISHING:
            return "Pesquera";
        case PATROL:
            return "Patrulla";
        default:
            return "Desconocido";
    }
}

const char *sideToString(Side side) {
    switch (side) {
        case LEFT_SIDE:
            return "Izquierda";
        case RIGHT_SIDE:
            return "Derecha";
        default:
            return "Desconocido";
    }
}
