#ifndef LCD_DISPLAY_H
#define LCD_DISPLAY_H

#include "ready_queue.h"
#include "ship_tasks.h"

#define LCD_DISPLAY_MAX_CHANNEL_SHIPS 4

/*
 * Inicializa toda la visualizacion fisica:
 * - 2 LCD I2C
 * - tira NeoPixel del canal
 * - LED de direccion
 * - LEDs de agujas
 */
void lcd_display_init(void);

/*
 * Actualiza LCD izquierdo y derecho.
 * current_ship se mantiene por compatibilidad con el codigo existente,
 * pero las llegadas reales se registran con lcd_display_show_arrival().
 */
void lcd_display_update(
    const ReadyQueue *left_queue,
    const ReadyQueue *right_queue,
    const ShipTask *current_ship
);

/*
 * Registra un barco que acaba de salir del canal.
 * Si el barco venia de la derecha, aparece en el LCD izquierdo.
 * Si venia de la izquierda, aparece en el LCD derecho.
 */
void lcd_display_show_arrival(const ShipTask *ship);

/*
 * Actualiza la tira LED que representa el canal.
 * positions[i] es la posicion logica dentro del canal.
 * types[i] es el tipo del barco en esa posicion.
 */
void lcd_display_update_channel(
    const int positions[],
    const ShipType types[],
    int count,
    int channel_length
);

/*
 * LED de direccion:
 * LEFT_SIDE  = apagado  = izquierda -> derecha
 * RIGHT_SIDE = encendido = derecha -> izquierda
 */
void lcd_display_set_direction(int direction);

/*
 * LEDs de agujas:
 * 0 = aguja arriba / entrada permitida
 * 1 = aguja abajo / entrada bloqueada
 */
void lcd_display_set_gates(int left_down, int right_down);

#endif