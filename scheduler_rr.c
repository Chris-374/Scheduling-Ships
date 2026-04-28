#include <stdio.h>
#include "scheduler_rr.h"

/*
 * Ejecuta un paso del algoritmo Round Robin sobre una cola de listos.
 *
 * Recibe:
 * - queue: cola de listos sobre la cual se va a calendarizar.
 * - quantum: cantidad maxima de tiempo que puede ejecutarse un barco
 *            en este turno.
 *
 * Funcionamiento general:
 * 1. Toma el primer barco de la cola.
 * 2. Lo ejecuta por un tiempo igual al quantum, o menos si termina antes.
 * 3. Si el barco aun tiene tiempo restante, lo vuelve a meter al final.
 * 4. Si ya termino, se elimina de la cola.
 */
void roundRobinStep(ReadyQueue *queue, int quantum) {
    /*
     * Validacion de seguridad.
     * Si la cola no existe, no se puede hacer nada.
     */
    if (queue == NULL) {
        return;
    }

    /*
     * El quantum debe ser mayor que cero.
     * Si fuera cero o negativo, el barco nunca avanzaria correctamente.
     */
    if (quantum <= 0) {
        printf("[ERROR] El quantum debe ser mayor que cero.\n");
        return;
    }

    /*
     * Si la cola esta vacia, no hay ningun barco listo para ejecutarse.
     */
    if (isQueueEmpty(queue)) {
        printf("%s vacia. No hay barcos para calendarizar.\n", queue->name);
        return;
    }

    /*
     * Se saca el primer barco de la cola.
     * Este es el barco que tiene el turno actual segun Round Robin.
     */
    Task current_task;
    if (!dequeue(queue, &current_task)) {
        return;
    }

    /*
     * Por defecto, el barco se ejecuta durante un quantum completo.
     */
    int executed_time = quantum;

    /*
     * Si el barco necesita menos tiempo que el quantum para terminar,
     * entonces solo se ejecuta por el tiempo que le falta.
     *
     * Ejemplo:
     * quantum = 4
     * remaining_time = 2
     * Entonces solo se ejecuta 2 unidades, no 4.
     */
    if (current_task.remaining_time < quantum) {
        executed_time = current_task.remaining_time;
    }

    /*
     * Se resta el tiempo ejecutado al tiempo restante del barco.
     */
    current_task.remaining_time -= executed_time;

    /*
     * Se muestra en pantalla que barco se esta ejecutando,
     * en cual cola esta y cuanto tiempo le queda.
     */
    printf("RR en %s: ejecutando %s por %d unidad(es). Restante: %d.\n",
           queue->name,
           current_task.name,
           executed_time,
           current_task.remaining_time);

    /*
     * Si el barco todavia no termina, se vuelve a insertar al final
     * de la cola. Esto es lo que caracteriza a Round Robin.
     */
    if (current_task.remaining_time > 0) {
        enqueue(queue, current_task);
    } else {
        /*
         * Si el tiempo restante llego a cero, el barco termino
         * y ya no vuelve a la cola.
         */
        printf("%s termino y sale de la cola.\n", current_task.name);
    }
}

/*
 * Ejecuta una simulacion de Round Robin usando dos colas de listos:
 * una para el lado izquierdo del canal y otra para el lado derecho.
 *
 * Recibe:
 * - left_queue: cola de barcos del lado izquierdo.
 * - right_queue: cola de barcos del lado derecho.
 * - quantum: tiempo maximo que cada barco puede ejecutarse por turno.
 *
 * Nota:
 * Por ahora esta funcion ejecuta un paso de RR en cada cola por ciclo.
 * Mas adelante, cuando se implemente el canal, el algoritmo de flujo
 * decidira de que lado deben pasar los barcos.
 */
void runRoundRobinTwoQueues(ReadyQueue *left_queue, ReadyQueue *right_queue, int quantum) {
    /*
     * Variable para contar los ciclos de la simulacion.
     * No afecta la logica del algoritmo, solo ayuda a visualizar.
     */
    int cycle = 1;

    printf("\n========== SIMULACION RR CON DOS COLAS ==========");
    printf("\nQuantum: %d\n", quantum);

    /*
     * La simulacion continua mientras al menos una de las dos colas
     * tenga barcos pendientes.
     */
    while (!isQueueEmpty(left_queue) || !isQueueEmpty(right_queue)) {
        printf("\n---------- Ciclo %d ----------\n", cycle);

        /*
         * Si la cola izquierda tiene barcos, se ejecuta un paso
         * de Round Robin sobre esa cola.
         */
        if (!isQueueEmpty(left_queue)) {
            roundRobinStep(left_queue, quantum);
        }

        /*
         * Si la cola derecha tiene barcos, se ejecuta un paso
         * de Round Robin sobre esa cola.
         */
        if (!isQueueEmpty(right_queue)) {
            roundRobinStep(right_queue, quantum);
        }

        /*
         * Se imprime el estado actual de ambas colas despues
         * de ejecutar el ciclo.
         */
        printQueue(left_queue);
        printQueue(right_queue);

        /*
         * Se avanza al siguiente ciclo de simulacion.
         */
        cycle++;
    }

    /*
     * Cuando ambas colas estan vacias, significa que todos los barcos
     * terminaron su ejecucion.
     */
    printf("\nTodas las colas quedaron vacias.\n");
}