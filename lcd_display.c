#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "driver/i2c_master.h"
#include "driver/gpio.h"
#include "esp_err.h"
#include "led_strip.h"

#include "lcd_display.h"

/* =========================
 * Pines configurables
 * =========================
 * Cambien estos pines segun el cableado real del ESP32-C6.
 */
#define I2C_SDA_GPIO 6
#define I2C_SCL_GPIO 7
#define I2C_PORT_NUM 0

#define LCD_LEFT_ADDR  0x27
#define LCD_RIGHT_ADDR 0x25

#define NEOPIXEL_GPIO       8
#define PHYSICAL_LED_COUNT  10

#define DIRECTION_LED_GPIO  15
#define LEFT_GATE_LED_GPIO  10
#define RIGHT_GATE_LED_GPIO 11

/* Brillo bajo para no exigir demasiada corriente a la tira. */
#define LED_BRIGHTNESS 32

#define LCD_BACKLIGHT 0x08
#define LCD_ENABLE    0x04
#define LCD_RS        0x01

#define LCD_COLS 16
#define ARRIVAL_HISTORY_SIZE 6

typedef struct {
    uint8_t address;
    i2c_master_dev_handle_t handle;
} LcdDevice;

static i2c_master_bus_handle_t bus_handle;
static LcdDevice lcd_left;
static LcdDevice lcd_right;
static int lcd_ready = 0;

static led_strip_handle_t strip_handle = NULL;
static int strip_ready = 0;
static int gpio_ready = 0;

static ShipType left_arrivals[ARRIVAL_HISTORY_SIZE];
static int left_arrival_count = 0;

static ShipType right_arrivals[ARRIVAL_HISTORY_SIZE];
static int right_arrival_count = 0;

/* =========================
 * Utilidades generales
 * ========================= */
static char ship_type_symbol(ShipType type)
{
    switch (type) {
        case NORMAL:
            return 'N';
        case FISHING:
            return 'P';
        case PATROL:
            return 'T';
        default:
            return '?';
    }
}

static void clear_line(char *line)
{
    for (int i = 0; i < LCD_COLS; i++) {
        line[i] = ' ';
    }

    line[LCD_COLS] = '\0';
}

static void copy_text_at(char *line, int start, const char *text)
{
    if (line == NULL || text == NULL || start < 0 || start >= LCD_COLS) {
        return;
    }

    int i = 0;

    while (text[i] != '\0' && start + i < LCD_COLS) {
        line[start + i] = text[i];
        i++;
    }
}

static void append_arrival(ShipType *history, int *count, ShipType type)
{
    if (history == NULL || count == NULL) {
        return;
    }

    if (*count < ARRIVAL_HISTORY_SIZE) {
        history[*count] = type;
        (*count)++;
        return;
    }

    for (int i = 1; i < ARRIVAL_HISTORY_SIZE; i++) {
        history[i - 1] = history[i];
    }

    history[ARRIVAL_HISTORY_SIZE - 1] = type;
}

static void build_arrival_line(const ShipType *history, int count, int from_right, char *line)
{
    clear_line(line);

    if (from_right) {
        line[0] = '<';
        copy_text_at(line, 1, "IN:");
    } else {
        copy_text_at(line, 0, "IN:");
        line[15] = '>';
    }

    int pos = from_right ? 4 : 3;

    for (int i = 0; i < count && pos < LCD_COLS; i++) {
        line[pos++] = ship_type_symbol(history[i]);

        if (pos < LCD_COLS) {
            line[pos++] = ' ';
        }
    }
}

static void build_left_queue_line(const ReadyQueue *queue, char *line)
{
    clear_line(line);
    copy_text_at(line, 0, "LQ:");

    int pos = 3;
    ReadyNode *current = queue != NULL ? queue->front : NULL;

    while (current != NULL && pos < 15) {
        if (current->ship != NULL) {
            line[pos++] = ship_type_symbol(current->ship->type);
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

    while (current != NULL && pos < LCD_COLS) {
        if (current->ship != NULL) {
            line[pos++] = ship_type_symbol(current->ship->type);
        }

        if (pos < LCD_COLS) {
            line[pos++] = ' ';
        }

        current = current->next;
    }
}

/* =========================
 * LCD I2C
 * ========================= */
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

    if (row < 0 || row > 1) {
        row = 0;
    }

    if (col < 0) {
        col = 0;
    }

    if (col >= LCD_COLS) {
        col = LCD_COLS - 1;
    }

    lcd_command(lcd, 0x80 | (row_offsets[row] + col));
}

static void lcd_print_line(LcdDevice *lcd, int row, const char *text)
{
    lcd_set_cursor(lcd, row, 0);

    int i = 0;

    while (text != NULL && text[i] != '\0' && i < LCD_COLS) {
        lcd_char(lcd, text[i]);
        i++;
    }

    while (i < LCD_COLS) {
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

static void lcd_init_all(void)
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

    lcd_print_line(&lcd_left, 0, "LQ:            >");
    lcd_print_line(&lcd_left, 1, "<IN:");

    lcd_print_line(&lcd_right, 0, "IN:            >");
    lcd_print_line(&lcd_right, 1, "<RQ:");

    printf("[LCD] LCDs listos. Izq=0x%02X Der=0x%02X\n",
           LCD_LEFT_ADDR,
           LCD_RIGHT_ADDR);
}

/* =========================
 * GPIO y NeoPixel
 * ========================= */
static void gpio_init_outputs(void)
{
    uint64_t pin_mask = (1ULL << DIRECTION_LED_GPIO) |
                        (1ULL << LEFT_GATE_LED_GPIO) |
                        (1ULL << RIGHT_GATE_LED_GPIO);

    gpio_config_t io_conf = {
        .pin_bit_mask = pin_mask,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };

    esp_err_t err = gpio_config(&io_conf);

    if (err != ESP_OK) {
        printf("[HW] Error configurando GPIOs: %s\n", esp_err_to_name(err));
        gpio_ready = 0;
        return;
    }

    gpio_ready = 1;
    gpio_set_level(DIRECTION_LED_GPIO, 0);
    gpio_set_level(LEFT_GATE_LED_GPIO, 0);
    gpio_set_level(RIGHT_GATE_LED_GPIO, 0);
}

static void neopixel_init(void)
{
    led_strip_config_t strip_config = {
        .strip_gpio_num = NEOPIXEL_GPIO,
        .max_leds = PHYSICAL_LED_COUNT,
    };

    led_strip_rmt_config_t rmt_config = {
        .resolution_hz = 10 * 1000 * 1000,
    };

    esp_err_t err = led_strip_new_rmt_device(
        &strip_config,
        &rmt_config,
        &strip_handle
    );

    if (err != ESP_OK) {
        printf("[NEOPIXEL] No se pudo inicializar tira LED: %s\n",
               esp_err_to_name(err));
        strip_ready = 0;
        return;
    }

    strip_ready = 1;
    led_strip_clear(strip_handle);
    printf("[NEOPIXEL] Tira lista. GPIO=%d LEDs=%d\n",
           NEOPIXEL_GPIO,
           PHYSICAL_LED_COUNT);
}

static void set_pixel_color(int led_index, ShipType type, int multiple)
{
    uint8_t r = 0;
    uint8_t g = 0;
    uint8_t b = 0;

    if (multiple) {
        r = LED_BRIGHTNESS;
        g = LED_BRIGHTNESS;
        b = LED_BRIGHTNESS;
    } else {
        switch (type) {
            case NORMAL:
                b = LED_BRIGHTNESS;
                break;
            case FISHING:
                g = LED_BRIGHTNESS;
                break;
            case PATROL:
                r = LED_BRIGHTNESS;
                break;
            default:
                r = LED_BRIGHTNESS;
                g = LED_BRIGHTNESS;
                b = 0;
                break;
        }
    }

    led_strip_set_pixel(strip_handle, led_index, r, g, b);
}

/* =========================
 * API publica
 * ========================= */
void lcd_display_init(void)
{
    lcd_init_all();
    gpio_init_outputs();
    neopixel_init();

    lcd_display_set_direction(LEFT_SIDE);
    lcd_display_set_gates(0, 0);
    lcd_display_update_channel(NULL, NULL, 0, 1);
}

void lcd_display_update(
    const ReadyQueue *left_queue,
    const ReadyQueue *right_queue,
    const ShipTask *current_ship
) {
    (void)current_ship;

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
     * fila 1 = llegadas desde derecha
     */
    build_left_queue_line(left_queue, left_top);
    build_arrival_line(left_arrivals, left_arrival_count, 1, left_bottom);

    /*
     * LCD derecho:
     * fila 0 = llegadas desde izquierda
     * fila 1 = cola derecha
     */
    build_arrival_line(right_arrivals, right_arrival_count, 0, right_top);
    build_right_queue_line(right_queue, right_bottom);

    lcd_print_line(&lcd_left, 0, left_top);
    lcd_print_line(&lcd_left, 1, left_bottom);

    lcd_print_line(&lcd_right, 0, right_top);
    lcd_print_line(&lcd_right, 1, right_bottom);
}

void lcd_display_show_arrival(const ShipTask *ship)
{
    if (ship == NULL) {
        return;
    }

    if (ship->side == RIGHT_SIDE) {
        append_arrival(left_arrivals, &left_arrival_count, ship->type);
    } else {
        append_arrival(right_arrivals, &right_arrival_count, ship->type);
    }
}

void lcd_display_update_channel(
    const int positions[],
    const ShipType types[],
    int count,
    int channel_length
) {
    if (!strip_ready || strip_handle == NULL) {
        return;
    }

    if (channel_length <= 0) {
        channel_length = 1;
    }

    ShipType led_type[PHYSICAL_LED_COUNT];
    int led_used[PHYSICAL_LED_COUNT];
    int led_multiple[PHYSICAL_LED_COUNT];

    for (int i = 0; i < PHYSICAL_LED_COUNT; i++) {
        led_type[i] = NORMAL;
        led_used[i] = 0;
        led_multiple[i] = 0;
        led_strip_set_pixel(strip_handle, i, 0, 0, 0);
    }

    for (int i = 0; i < count; i++) {
        if (positions == NULL || types == NULL) {
            break;
        }

        if (positions[i] < 0 || positions[i] >= channel_length) {
            continue;
        }

        int led_index = (positions[i] * PHYSICAL_LED_COUNT) / channel_length;

        if (led_index < 0) {
            led_index = 0;
        }

        if (led_index >= PHYSICAL_LED_COUNT) {
            led_index = PHYSICAL_LED_COUNT - 1;
        }

        if (led_used[led_index]) {
            led_multiple[led_index] = 1;
        } else {
            led_type[led_index] = types[i];
            led_used[led_index] = 1;
        }
    }

    for (int i = 0; i < PHYSICAL_LED_COUNT; i++) {
        if (led_used[i]) {
            set_pixel_color(i, led_type[i], led_multiple[i]);
        }
    }

    led_strip_refresh(strip_handle);
}

void lcd_display_set_direction(int direction)
{
    if (!gpio_ready) {
        return;
    }

    gpio_set_level(DIRECTION_LED_GPIO, direction == RIGHT_SIDE ? 1 : 0);
}

void lcd_display_set_gates(int left_down, int right_down)
{
    if (!gpio_ready) {
        return;
    }

    gpio_set_level(LEFT_GATE_LED_GPIO, left_down ? 1 : 0);
    gpio_set_level(RIGHT_GATE_LED_GPIO, right_down ? 1 : 0);
}