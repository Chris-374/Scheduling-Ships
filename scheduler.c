#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "scheduler.h"

/*
 * Crea una nueva tarea/barco con los datos básicos.
 *
 * Recibe:
 * - id: identificador único del barco.
 * - name: nombre del barco.
 * - type: tipo de barco, por ejemplo NORMAL, FISHING o PATROL.
 * - side: lado del canal donde inicia el barco.
 * - burst_time: tiempo total que el barco necesita para terminar.
 *
 * Retorna:
 * - Una estructura Task ya inicializada.
 *
 * Nota:
 * remaining_time inicia con el mismo valor que burst_time,
 * porque al crear el barco todavía no se ha ejecutado nada.
 */
Task createTask(int id, const char *name, ShipType type, Side side, int burst_time) {
    Task task;

    /*
     * Se asignan los datos principales del barco.
     */
    task.id = id;

    /*
     * Se copia el nombre del barco de forma segura.
     * NAME_SIZE - 1 deja espacio para el caracter final '\0'.
     */
    strncpy(task.name, name, NAME_SIZE - 1);
    task.name[NAME_SIZE - 1] = '\0';

    task.type = type;
    task.side = side;

    /*
     * burst_time representa el tiempo total que necesita el barco.
     * remaining_time representa cuanto tiempo le falta por ejecutar.
     */
    task.burst_time = burst_time;
    task.remaining_time = burst_time;

    /*
     * Estos campos no se usan todavía con Round Robin.
     * Se dejan inicializados para usarlos despues con otros calendarizadores.
     *
     * priority -> se usara con calendarizacion por prioridad.
     * deadline -> se usara con EDF.
     */
    task.priority = 0;
    task.deadline = 0;

    return task;
}

/*
 * Inicializa una cola de listos.
 *
 * Recibe:
 * - queue: puntero a la cola que se desea inicializar.
 * - name: nombre descriptivo de la cola.
 * - capacity: cantidad maxima de barcos que puede tener la cola.
 *
 * En este proyecto la capacidad normalmente seria maximo 4,
 * porque el enunciado indica que cada lado del canal puede tener
 * una cantidad limitada de barcos en la cola.
 */
void initQueue(ReadyQueue *queue, const char *name, int capacity) {
    /*
     * Validacion de seguridad.
     * Si la cola no existe, no se puede inicializar.
     */
    if (queue == NULL) {
        return;
    }

    /*
     * Al inicio la cola esta vacia, por eso front y rear son NULL.
     */
    queue->front = NULL;
    queue->rear = NULL;

    /*
     * size indica cuantos barcos hay actualmente en la cola.
     */
    queue->size = 0;

    /*
     * capacity indica la cantidad maxima de barcos permitidos.
     */
    queue->capacity = capacity;

    /*
     * Se guarda el nombre de la cola de forma segura.
     */
    strncpy(queue->name, name, NAME_SIZE - 1);
    queue->name[NAME_SIZE - 1] = '\0';
}

/*
 * Verifica si una cola esta vacia.
 *
 * Retorna:
 * - 1 si la cola no existe o si no tiene elementos.
 * - 0 si la cola tiene al menos un barco.
 */
int isQueueEmpty(const ReadyQueue *queue) {
    return queue == NULL || queue->size == 0;
}

/*
 * Inserta un barco al final de la cola.
 *
 * Recibe:
 * - queue: cola donde se quiere insertar el barco.
 * - task: barco/tarea que se desea insertar.
 *
 * Retorna:
 * - 1 si se pudo insertar correctamente.
 * - 0 si hubo algun error.
 *
 * Esta funcion implementa el comportamiento normal de una cola:
 * el nuevo barco entra por atras.
 */
int enqueue(ReadyQueue *queue, Task task) {
    /*
     * Si la cola no existe, no se puede insertar nada.
     */
    if (queue == NULL) {
        return 0;
    }

    /*
     * Si la cola ya llego a su capacidad maxima, no se permite
     * agregar otro barco.
     */
    if (queue->size >= queue->capacity) {
        printf("[ERROR] La %s esta llena. No se pudo agregar el barco %s.\n", queue->name, task.name);
        return 0;
    }

    /*
     * Se reserva memoria dinamica para un nuevo nodo de la cola.
     * Cada nodo guarda una Task y un puntero al siguiente nodo.
     */
    Node *new_node = (Node *)malloc(sizeof(Node));

    /*
     * Si malloc falla, se evita usar memoria invalida.
     */
    if (new_node == NULL) {
        printf("[ERROR] No se pudo reservar memoria para el barco %s.\n", task.name);
        return 0;
    }

    /*
     * Se guarda la tarea dentro del nodo.
     * Como sera el ultimo nodo de la cola, su siguiente es NULL.
     */
    new_node->task = task;
    new_node->next = NULL;

    /*
     * Caso 1:
     * Si rear es NULL, significa que la cola estaba vacia.
     * Entonces el nuevo nodo es al mismo tiempo el primero y el ultimo.
     */
    if (queue->rear == NULL) {
        queue->front = new_node;
        queue->rear = new_node;
    } else {
        /*
         * Caso 2:
         * Si la cola ya tenia elementos, el nodo nuevo se conecta
         * despues del ultimo nodo actual.
         *
         * Luego rear se actualiza para apuntar al nuevo ultimo nodo.
         */
        queue->rear->next = new_node;
        queue->rear = new_node;
    }

    /*
     * Se aumenta el contador de elementos de la cola.
     */
    queue->size++;

    return 1;
}

/*
 * Saca el primer barco de la cola.
 *
 * Recibe:
 * - queue: cola de la cual se desea extraer un barco.
 * - task_out: puntero donde se guardara el barco extraido.
 *
 * Retorna:
 * - 1 si se pudo sacar un barco correctamente.
 * - 0 si la cola estaba vacia o hubo algun error.
 *
 * Esta funcion implementa la salida por el frente de la cola.
 */
int dequeue(ReadyQueue *queue, Task *task_out) {
    /*
     * Validaciones:
     * - La cola debe existir.
     * - task_out debe ser un puntero valido.
     * - La cola debe tener al menos un nodo.
     */
    if (queue == NULL || task_out == NULL || queue->front == NULL) {
        return 0;
    }

    /*
     * temp apunta al primer nodo de la cola.
     * Ese nodo es el que se va a eliminar.
     */
    Node *temp = queue->front;

    /*
     * Se copia la tarea del primer nodo hacia task_out,
     * para que quien llamo la funcion pueda usarla.
     */
    *task_out = temp->task;

    /*
     * El nuevo frente de la cola pasa a ser el siguiente nodo.
     */
    queue->front = queue->front->next;

    /*
     * Si despues de avanzar front queda en NULL, significa que
     * la cola quedo vacia.
     *
     * En ese caso rear tambien debe quedar en NULL.
     */
    if (queue->front == NULL) {
        queue->rear = NULL;
    }

    /*
     * Se libera la memoria del nodo que salio de la cola.
     */
    free(temp);

    /*
     * Se reduce el contador de elementos.
     */
    queue->size--;

    return 1;
}

/*
 * Imprime el contenido actual de una cola.
 *
 * Recibe:
 * - queue: cola que se quiere mostrar.
 *
 * Esta funcion sirve principalmente para depurar y ver como
 * cambia la cola despues de cada paso del calendarizador.
 */
void printQueue(const ReadyQueue *queue) {
    /*
     * Si la cola no existe, no hay nada que imprimir.
     */
    if (queue == NULL) {
        return;
    }

    /*
     * Se imprime el nombre de la cola y su ocupacion actual.
     * Ejemplo:
     * Cola izquierda [2/4]
     */
    printf("\n%s [%d/%d]: ", queue->name, queue->size, queue->capacity);

    /*
     * Si no hay ningun nodo al frente, la cola esta vacia.
     */
    if (queue->front == NULL) {
        printf("vacia\n");
        return;
    }

    /*
     * current se usa para recorrer la lista enlazada desde el frente.
     */
    Node *current = queue->front;

    /*
     * Se recorren todos los nodos hasta llegar al final.
     */
    while (current != NULL) {
        /*
         * Se imprime la informacion principal de cada barco:
         * - nombre
         * - id
         * - tipo
         * - tiempo restante
         */
        printf("%s(id=%d, tipo=%s, restante=%d)",
               current->task.name,
               current->task.id,
               shipTypeToString(current->task.type),
               current->task.remaining_time);

        /*
         * Si hay otro nodo despues, se imprime una flecha para
         * visualizar el orden de la cola.
         */
        if (current->next != NULL) {
            printf(" -> ");
        }

        /*
         * Se avanza al siguiente nodo.
         */
        current = current->next;
    }

    printf("\n");
}

/*
 * Libera todos los nodos de una cola.
 *
 * Esta funcion debe llamarse al final del programa para evitar
 * fugas de memoria.
 */
void destroyQueue(ReadyQueue *queue) {
    /*
     * Si la cola no existe, no hay nada que liberar.
     */
    if (queue == NULL) {
        return;
    }

    /*
     * Variable temporal donde se guarda cada barco descartado.
     * No se usa despues porque el objetivo solo es vaciar la cola.
     */
    Task discarded;

    /*
     * Mientras dequeue logre sacar elementos, se siguen eliminando nodos.
     * dequeue ya se encarga de hacer free() internamente.
     */
    while (dequeue(queue, &discarded)) {
        /* Se libera cada nodo mediante dequeue. */
    }
}

/*
 * Convierte el tipo de barco a texto.
 *
 * Esto sirve para imprimir nombres legibles en pantalla,
 * en vez de mostrar solo numeros como 0, 1 o 2.
 */
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

/*
 * Convierte el lado del canal a texto.
 *
 * Esto sirve para mostrar si un barco pertenece al lado izquierdo
 * o derecho del canal.
 */
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