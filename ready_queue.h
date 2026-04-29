#ifndef READY_QUEUE_H
#define READY_QUEUE_H

#include "ship_tasks.h"

/*
 * Cantidad maxima de barcos permitidos en cada cola.
 * Segun el proyecto, cada lado del canal puede tener maximo 4 barcos.
 */
#define MAX_QUEUE_SIZE 4

/*
 * Nodo de la cola.
 *
 * Cada nodo guarda un puntero a un ShipTask.
 * Es decir, no guarda una copia del barco, sino la direccion
 * del barco real que tambien tiene asociada su task de FreeRTOS.
 */
typedef struct ReadyNode {
    ShipTask *ship;
    struct ReadyNode *next;
} ReadyNode;

/*
 * Cola de listos.
 *
 * Representa una fila de barcos esperando en un lado del canal.
 *
 * front:
 *   apunta al primer barco de la cola.
 *
 * rear:
 *   apunta al ultimo barco de la cola.
 *
 * size:
 *   cantidad actual de barcos en la cola.
 *
 * capacity:
 *   capacidad maxima permitida.
 *
 * name:
 *   nombre de la cola, por ejemplo "Cola izquierda".
 */
typedef struct {
    ReadyNode *front;
    ReadyNode *rear;

    int size;
    int capacity;

    char name[NAME_SIZE];
} ReadyQueue;

/*
 * Inicializa una cola de listos.
 */
void initQueue(ReadyQueue *queue, const char *name, int capacity);

/*
 * Revisa si la cola esta vacia.
 *
 * Retorna:
 * 1 si esta vacia.
 * 0 si tiene al menos un barco.
 */
int isQueueEmpty(const ReadyQueue *queue);

/*
 * Inserta un barco al final de la cola.
 *
 * Se usa para FCFS y RR, porque ambos respetan
 * el orden normal de llegada.
 */
int enqueue(ReadyQueue *queue, ShipTask *ship);

/*
 * Saca el primer barco de la cola.
 *
 * ship_out es un doble puntero porque queremos devolver
 * un ShipTask* desde la funcion.
 */
int dequeue(ReadyQueue *queue, ShipTask **ship_out);

/*
 * Imprime el estado actual de la cola.
 *
 * Sirve para ver el orden de los barcos durante las pruebas.
 */
void printQueue(const ReadyQueue *queue);

/*
 * Libera los nodos internos de la cola.
 *
 * Importante:
 * Esta funcion libera los nodos de la cola, pero NO elimina
 * las tasks reales de FreeRTOS ni libera los ShipTask.
 */
void destroyQueue(ReadyQueue *queue);

#endif