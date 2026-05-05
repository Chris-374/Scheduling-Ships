#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "driver/i2c_master.h"
#include "esp_err.h"

#include "lcd_display.h"

#define I2C_SDA_GPIO 6
#define I2C_SCL_GPIO 7
#define I2C_PORT_NUM 0

#define LCD_LEFT_ADDR  0x27
#define LCD_RIGHT_ADDR 0x25

#define LCD_BACKLIGHT 0x08
#define LCD_ENABLE    0x04
#define LCD_RS        0x01

typedef struct {
    uint8_t address;
    i2c_master_dev_handle_t handle;
} LcdDevice;

static i2c_master_bus_handle_t bus_handle;
static LcdDevice lcd_left;
static LcdDevice lcd_right;
static int lcd_ready = 0;

static void lcd_write_raw(LcdDevice *lcd, uint8_t data)
{
    if (!lcd_ready || lcd == NULL) {
        return;
    }

    esp_err_t err = i2c_master_transmit(lcd->handle, &data, 1, 1000);

    if (err != ESP_OK) {
        printf("[LCD] Error escribiendo a 0x%02X: %s\n",
               lcd->address,
               esp_err_to_name(err));
    }
}

static void lcd_pulse_enable(LcdDevice *lcd, uint8_t data)
{
    lcd_write_raw(lcd, data | LCD_ENABLE);
    vTaskDelay(pdMS_TO_TICKS(1));

    lcd_write_raw(lcd, data & ~LCD_ENABLE);
    vTaskDelay(pdMS_TO_TICKS(1));
}

static void lcd_send_nibble(LcdDevice *lcd, uint8_t nibble, uint8_t mode)
{
    uint8_t data = (nibble << 4) | LCD_BACKLIGHT | mode;
    lcd_write_raw(lcd, data);
    lcd_pulse_enable(lcd, data);
}

static void lcd_send_byte(LcdDevice *lcd, uint8_t value, uint8_t mode)
{
    lcd_send_nibble(lcd, value >> 4, mode);
    lcd_send_nibble(lcd, value & 0x0F, mode);
}

static void lcd_command(LcdDevice *lcd, uint8_t command)
{
    lcd_send_byte(lcd, command, 0);
}

static void lcd_char(LcdDevice *lcd, char c)
{
    lcd_send_byte(lcd, (uint8_t)c, LCD_RS);
}

static void lcd_clear(LcdDevice *lcd)
{
    lcd_command(lcd, 0x01);
    vTaskDelay(pdMS_TO_TICKS(2));
}

static void lcd_set_cursor(LcdDevice *lcd, int row, int col)
{
    int row_offsets[] = {0x00, 0x40};
    lcd_command(lcd, 0x80 | (row_offsets[row] + col));
}

static void lcd_print_line(LcdDevice *lcd, int row, const char *text)
{
    lcd_set_cursor(lcd, row, 0);

    int i = 0;

    while (text[i] != '\0' && i < 16) {
        lcd_char(lcd, text[i]);
        i++;
    }

    while (i < 16) {
        lcd_char(lcd, ' ');
        i++;
    }
}

static void lcd_init_device(LcdDevice *lcd)
{
    vTaskDelay(pdMS_TO_TICKS(50));

    lcd_send_nibble(lcd, 0x03, 0);
    vTaskDelay(pdMS_TO_TICKS(5));

    lcd_send_nibble(lcd, 0x03, 0);
    vTaskDelay(pdMS_TO_TICKS(5));

    lcd_send_nibble(lcd, 0x03, 0);
    vTaskDelay(pdMS_TO_TICKS(1));

    lcd_send_nibble(lcd, 0x02, 0);

    lcd_command(lcd, 0x28);
    lcd_command(lcd, 0x0C);
    lcd_command(lcd, 0x06);
    lcd_clear(lcd);
}

static int lcd_add_device(LcdDevice *lcd, uint8_t address)
{
    lcd->address = address;

    i2c_device_config_t lcd_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = address,
        .scl_speed_hz = 100000,
    };

    esp_err_t err = i2c_master_bus_add_device(
        bus_handle,
        &lcd_config,
        &lcd->handle
    );

    if (err != ESP_OK) {
        printf("[LCD] No se pudo agregar LCD 0x%02X: %s\n",
               address,
               esp_err_to_name(err));
        return 0;
    }

    return 1;
}

static void clear_line(char *line)
{
    for (int i = 0; i < 16; i++) {
        line[i] = ' ';
    }

    line[16] = '\0';
}

static void copy_text_at(char *line, int start, const char *text)
{
    int i = 0;

    while (text[i] != '\0' && start + i < 16) {
        line[start + i] = text[i];
        i++;
    }
}

static void get_short_ship_name(const ShipTask *ship, char *out, int out_size)
{
    if (out == NULL || out_size <= 0) {
        return;
    }

    out[0] = '\0';

    if (ship == NULL || ship->name[0] == '\0') {
        return;
    }

    int i = 0;

    while (ship->name[i] != '\0' &&
           ship->name[i] != '_' &&
           i < out_size - 1) {
        out[i] = ship->name[i];
        i++;
    }

    out[i] = '\0';
}

static void build_left_queue_line(const ReadyQueue *queue, char *line)
{
    clear_line(line);
    copy_text_at(line, 0, "LQ:");

    int pos = 3;
    ReadyNode *current = queue != NULL ? queue->front : NULL;

    while (current != NULL && pos < 15) {
        char name[8];
        get_short_ship_name(current->ship, name, sizeof(name));

        for (int i = 0; name[i] != '\0' && pos < 15; i++) {
            line[pos++] = name[i];
        }

        if (pos < 15) {
            line[pos++] = ' ';
        }

        current = current->next;
    }

    line[15] = '>';
}

static void build_right_queue_line(const ReadyQueue *queue, char *line)
{
    clear_line(line);
    line[0] = '<';
    copy_text_at(line, 1, "RQ:");

    int pos = 4;
    ReadyNode *current = queue != NULL ? queue->front : NULL;

    while (current != NULL && pos < 16) {
        char name[8];
        get_short_ship_name(current->ship, name, sizeof(name));

        for (int i = 0; name[i] != '\0' && pos < 16; i++) {
            line[pos++] = name[i];
        }

        if (pos < 16) {
            line[pos++] = ' ';
        }

        current = current->next;
    }
}

static void build_incoming_from_right(const ShipTask *ship, char *line)
{
    clear_line(line);
    line[0] = '<';
    copy_text_at(line, 1, "IN:");

    if (ship != NULL && ship->side == RIGHT_SIDE) {
        char name[8];
        get_short_ship_name(ship, name, sizeof(name));
        copy_text_at(line, 4, name);
    }
}

static void build_incoming_from_left(const ShipTask *ship, char *line)
{
    clear_line(line);
    copy_text_at(line, 0, "IN:");

    if (ship != NULL && ship->side == LEFT_SIDE) {
        char name[8];
        get_short_ship_name(ship, name, sizeof(name));
        copy_text_at(line, 3, name);
    }

    line[15] = '>';
}

void lcd_display_init(void)
{
    printf("[LCD] Inicializando LCDs...\n");

    i2c_master_bus_config_t bus_config = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .i2c_port = I2C_PORT_NUM,
        .sda_io_num = I2C_SDA_GPIO,
        .scl_io_num = I2C_SCL_GPIO,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };

    esp_err_t err = i2c_new_master_bus(&bus_config, &bus_handle);

    if (err != ESP_OK) {
        printf("[LCD] Error creando bus I2C: %s\n", esp_err_to_name(err));
        lcd_ready = 0;
        return;
    }

    if (!lcd_add_device(&lcd_left, LCD_LEFT_ADDR)) {
        lcd_ready = 0;
        return;
    }

    if (!lcd_add_device(&lcd_right, LCD_RIGHT_ADDR)) {
        lcd_ready = 0;
        return;
    }

    lcd_ready = 1;

    lcd_init_device(&lcd_left);
    lcd_init_device(&lcd_right);

    lcd_print_line(&lcd_left, 0, "LCD IZQ OK");
    lcd_print_line(&lcd_left, 1, "0x27");

    lcd_print_line(&lcd_right, 0, "LCD DER OK");
    lcd_print_line(&lcd_right, 1, "0x25");

    vTaskDelay(pdMS_TO_TICKS(1000));

    printf("[LCD] LCDs listos.\n");
}

void lcd_display_update(
    const ReadyQueue *left_queue,
    const ReadyQueue *right_queue,
    const ShipTask *current_ship
) {
    if (!lcd_ready) {
        return;
    }

    char left_top[17];
    char left_bottom[17];
    char right_top[17];
    char right_bottom[17];

    /*
     * LCD izquierdo:
     * fila 0 = cola izquierda
     * fila 1 = barco que viene desde derecha
     */
    build_left_queue_line(left_queue, left_top);
    build_incoming_from_right(current_ship, left_bottom);

    /*
     * LCD derecho:
     * filas intercambiadas como pediste
     * fila 0 = barco que viene desde izquierda
     * fila 1 = cola derecha
     */
    build_incoming_from_left(current_ship, right_top);
    build_right_queue_line(right_queue, right_bottom);

    lcd_print_line(&lcd_left, 0, left_top);
    lcd_print_line(&lcd_left, 1, left_bottom);

    lcd_print_line(&lcd_right, 0, right_top);
    lcd_print_line(&lcd_right, 1, right_bottom);
}