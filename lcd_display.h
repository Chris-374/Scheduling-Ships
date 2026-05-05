#ifndef LCD_DISPLAY_H
#define LCD_DISPLAY_H

#include "ready_queue.h"
#include "ship_tasks.h"

void lcd_display_init(void);

void lcd_display_update(
    const ReadyQueue *left_queue,
    const ReadyQueue *right_queue,
    const ShipTask *current_ship
);

#endif