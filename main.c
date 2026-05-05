#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "canal.h"
#include "ship_tasks.h"
#include "ready_queue.h"
#include "scheduler_rr_freertos.h"
#include "scheduler_priority_freertos.h"
#include "scheduler_sjf_freertos.h"
#include "scheduler_strn_freertos.h"
#include "scheduler_fcfs_freertos.h"
#include "scheduler_edf_freertos.h"
#include "lcd_display.h"

/*
 * Barcos reales.
 *
 * Por ahora son globales/static para que sigan existiendo durante
 * toda la ejecucion del programa.
 */
static ShipTask ship1;
static ShipTask ship2;
static ShipTask ship3;
static ShipTask ship4;

/*
 * Colas de listos.
 *
 * Simulan los dos lados del canal:
 * - izquierda
 * - derecha
 */
static ReadyQueue left_queue;
static ReadyQueue right_queue;

/*
 * Calendarizadores disponibles.
 *
 * El enunciado pide que el calendarizador sea un parametro
 * de cada ejecucion. Por ahora lo escogemos con un define.
 *
 * Mas adelante esto puede venir de:
 * - archivo de configuracion
 * - menu por consola
 * - boton/interfaz
 */
typedef enum {
    SCHEDULER_RR = 0,
    SCHEDULER_PRIORITY = 1,
    SCHEDULER_SJF = 2,
    SCHEDULER_STRN = 3,
    SCHEDULER_FCFS = 4,
    SCHEDULER_EDF = 5
} SchedulerType;

/*
 * Cambiar aqui para probar:
 *
 * SCHEDULER_RR
 * SCHEDULER_PRIORITY
 * SCHEDULER_SJF
 * SCHEDULER_STRN
 * SCHEDULER_FCFS
 * SCHEDULER_EDF
 */
#define SELECTED_SCHEDULER SCHEDULER_RR

/*
 * Esta es la task del calendarizador.
 *
 * Su trabajo es ejecutar el algoritmo seleccionado sobre las dos colas.
 */
void schedulerTask(void *pvParameters) {
    // int quantum = 2; // Se comenta porque no se usará mientras probamos el canal

    printf("\n[SCHEDULER] Iniciando calendarizador\n");

    printQueue(&left_queue);
    printQueue(&right_queue);

    /* 
     * SECCIÓN COMENTADA TEMPORALMENTE:
     * Dejamos intacto el código original, pero comentado para que no vacíe
     * las colas antes de que el puente pueda actuar.
     */
    /*
    switch (SELECTED_SCHEDULER) {
        case SCHEDULER_RR:
            printf("\n[SCHEDULER] Calendarizador seleccionado: Round Robin\n");

            runRoundRobinFreeRTOSTwoQueues(
                &left_queue,
                &right_queue,
                quantum
            );
            break;

        case SCHEDULER_PRIORITY:
            printf("\n[SCHEDULER] Calendarizador seleccionado: Prioridad\n");

            runPriorityFreeRTOSTwoQueues(
                &left_queue,
                &right_queue
            );
            break;

        case SCHEDULER_SJF:
            printf("\n[SCHEDULER] Calendarizador seleccionado: SJF\n");

            runSJFFreeRTOSTwoQueues(
                &left_queue,
                &right_queue
            );
            break;

        case SCHEDULER_STRN:
            printf("\n[SCHEDULER] Calendarizador seleccionado: STRN\n");

            runSTRNFreeRTOSTwoQueues(
                &left_queue,
                &right_queue
            );
            break;

        case SCHEDULER_FCFS:
            printf("\n[SCHEDULER] Calendarizador seleccionado: FCFS\n");

            runFCFSFreeRTOSTwoQueues(
                &left_queue,
                &right_queue
            );
            break;

        case SCHEDULER_EDF:
            printf("\n[SCHEDULER] Calendarizador seleccionado: EDF\n");

            runEDFFreeRTOSTwoQueues(
                &left_queue,
                &right_queue
            );
            break;

        default:
            printf("\n[ERROR] Calendarizador desconocido.\n");
            break;
    }
    */

    // LLAMADA A SU CONTROL DE FLUJO:
    printf("\n[PRUEBA] Ejecutando control del Canal (Equidad W=2)\n");
    run_channel_equity(&left_queue, &right_queue, 2);

    printf("\n[SCHEDULER] Calendarizacion terminada.\n");

    /*
     * Se limpian los nodos de las colas.
     * Esto no elimina las tasks reales, solo limpia las colas.
     */
    destroyQueue(&left_queue);
    destroyQueue(&right_queue);

    vTaskDelete(NULL);
}

/*
 * Punto de entrada de ESP-IDF.
 */
void app_main(void) {
    printf("\n===== SCHEDULING SHIPS CON TASKS REALES =====\n");

    /*
     * Inicializamos las dos colas de listos.
     */
    initQueue(&left_queue, "Cola izquierda");
    initQueue(&right_queue, "Cola derecha");

    lcd_display_init();

    /*
     * Creamos barcos como tasks reales de FreeRTOS.
     *
     * Parametros:
     *
     * createShipTask(
     *     &barco,
     *     id,
     *     nombre,
     *     tipo,
     *     lado,
     *     tiempo_total,
     *     prioridad,
     *     deadline
     * );
     *
     * Para prioridad:
     * menor numero = mayor prioridad.
     *
     * Para SJF:
     * menor tiempo_total = corre primero.
     *
     * Para STRN:
     * menor tiempo restante = corre primero.
     *
     * Para FCFS:
     * primero que entra a la cola = primero que corre.
     *
     * Para EDF:
     * menor deadline = corre primero.
     */
    createShipTask(
        &ship1,
        1,
        "L1_Normal",
        NORMAL,
        LEFT_SIDE,
        5,
        4,
        12
    );

    createShipTask(
        &ship2,
        2,
        "L2_Patrulla",
        PATROL,
        LEFT_SIDE,
        3,
        1,
        5
    );

    createShipTask(
        &ship3,
        3,
        "R1_Pesquera",
        FISHING,
        RIGHT_SIDE,
        4,
        2,
        8
    );

    createShipTask(
        &ship4,
        4,
        "R2_Normal",
        NORMAL,
        RIGHT_SIDE,
        6,
        5,
        15
    );

    /*
     * Insertamos los barcos en las colas.
     *
     * Importante:
     * Estamos metiendo punteros a ShipTask reales.
     *
     * Cola izquierda:
     * - L1_Normal, deadline 12
     * - L2_Patrulla, deadline 5
     *
     * Cola derecha:
     * - R1_Pesquera, deadline 8
     * - R2_Normal, deadline 15
     *
     * Para EDF este valor importa directamente.
     */
    enqueue(&left_queue, &ship1);
    enqueue(&left_queue, &ship2);

    enqueue(&right_queue, &ship3);
    enqueue(&right_queue, &ship4);


    lcd_display_update(&left_queue, &right_queue, NULL);
    
    /*
     * Creamos la task del calendarizador.
     *
     * Esta task sera la que despierte a los barcos segun
     * el algoritmo seleccionado.
     */
    BaseType_t result = xTaskCreatePinnedToCore(
        schedulerTask,
        "SchedulerTask",
        4096,
        NULL,
        2,
        NULL,
        tskNO_AFFINITY
    );

    if (result != pdPASS) {
        printf("[ERROR] No se pudo crear la task del calendarizador.\n");
        return;
    }

    printf("[MAIN] Calendarizador creado correctamente.\n");
}