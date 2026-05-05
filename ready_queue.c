#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ready_queue.h"

/*
 * Inicializa una cola de listos.
 *
 * Esta cola representa los barcos que estan esperando
 * en un lado del canal.
 */
void initQueue(ReadyQueue *queue, const char *name) {
    if (queue == NULL) {
        return;
    }

    queue->front = NULL;
    queue->rear = NULL;

    queue->size = 0;

    strncpy(queue->name, name, NAME_SIZE - 1);
    queue->name[NAME_SIZE - 1] = '\0';
}

/*
 * Verifica si una cola esta vacia.
 *
 * Retorna:
 * 1 si la cola esta vacia o si el puntero es NULL.
 * 0 si la cola tiene al menos un barco.
 */
int isQueueEmpty(const ReadyQueue *queue) {
    return queue == NULL || queue->size == 0;
}

/*
 * Inserta un barco al final de la cola.
 *
 * Importante:
 * La cola NO guarda una copia del barco.
 * Guarda un puntero al ShipTask real.
 *
 * Esto permite que los cambios sobre remaining_time, state
 * y handle sean cambios sobre el barco real.
 */
int enqueue(ReadyQueue *queue, ShipTask *ship) {
    if (queue == NULL || ship == NULL) {
        return 0;
    }

    ReadyNode *new_node = (ReadyNode *)malloc(sizeof(ReadyNode));

    if (new_node == NULL) {
        printf("[ERROR] No se pudo reservar memoria para el barco %s.\n",
               ship->name);
        return 0;
    }

    new_node->ship = ship;
    new_node->next = NULL;

    /*
     * Si rear es NULL, significa que la cola estaba vacia.
     * Entonces el nuevo nodo sera tanto el primero como el ultimo.
     */
    if (queue->rear == NULL) {
        queue->front = new_node;
        queue->rear = new_node;
    } else {
        /*
         * Si ya habia barcos, el nuevo nodo se pega despues
         * del ultimo nodo actual.
         */
        queue->rear->next = new_node;
        queue->rear = new_node;
    }

    queue->size++;

    return 1;
}

/*
 * Saca el primer barco de la cola.
 *
 * Como la cola guarda punteros, esta funcion devuelve un ShipTask*
 * usando el parametro ship_out.
 */
int dequeue(ReadyQueue *queue, ShipTask **ship_out) {
    if (queue == NULL || ship_out == NULL || queue->front == NULL) {
        return 0;
    }

    ReadyNode *temp = queue->front;

    /*
     * Se devuelve el puntero al barco real.
     */
    *ship_out = temp->ship;

    /*
     * El frente de la cola avanza al siguiente nodo.
     */
    queue->front = queue->front->next;

    /*
     * Si despues de sacar el nodo la cola quedo vacia,
     * rear tambien debe quedar en NULL.
     */
    if (queue->front == NULL) {
        queue->rear = NULL;
    }

    /*
     * Se libera el nodo de la cola.
     * No se libera el ShipTask, porque el barco real sigue existiendo.
     */
    free(temp);

    queue->size--;

    return 1;
}

/*
 * Imprime el contenido actual de la cola.
 *
 * Sirve para ver en que orden estan los barcos.
 */
void printQueue(const ReadyQueue *queue) {
    if (queue == NULL) {
        return;
    }

    printf("\n%s [%d barcos]: ",
           queue->name,
           queue->size);

    if (queue->front == NULL) {
        printf("vacia\n");
        return;
    }

    ReadyNode *current = queue->front;

    while (current != NULL) {
        ShipTask *ship = current->ship;

        if (ship != NULL) {
            printf("%s(id=%d, tipo=%s, restante=%d, prioridad=%d, deadline=%d)",
                   ship->name,
                   ship->id,
                   shipTypeToString(ship->type),
                   ship->remaining_time,
                   ship->priority,
                   ship->deadline);
        } else {
            printf("[barco NULL]");
        }

        if (current->next != NULL) {
            printf(" -> ");
        }

        current = current->next;
    }

    printf("\n");
}

/*
 * Libera todos los nodos de la cola.
 *
 * Importante:
 * Esta funcion NO elimina las tasks de FreeRTOS.
 * Solo limpia la estructura de cola.
 *
 * Los ShipTask reales existen fuera de la cola.
 */
void destroyQueue(ReadyQueue *queue) {
    if (queue == NULL) {
        return;
    }

    ShipTask *discarded;

    while (dequeue(queue, &discarded)) {
        /*
         * dequeue libera cada nodo.
         * No hacemos free(discarded), porque discarded apunta
         * a un ShipTask real creado fuera de la cola.
         */
    }

    queue->front = NULL;
    queue->rear = NULL;
    queue->size = 0;
}