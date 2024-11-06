// main.c

#include <stdio.h>
#include <inttypes.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_system.h"
#include "esp_cpu.h"
#include "esp_log.h"
#include "driver/ledc.h"
#include "driver/gpio.h"
#include "motor.h"

// BLE
#include "gap.h"
#include "gatt_svr.h"
#include "nvs_flash.h"
#include "esp_bt.h"

#include "ws2812_control.h"
#include "motor.h"
#include "gpio_interrupt.h"

#include "driver/usb_serial_jtag.h"
#include "esp_vfs_dev.h"



static const char *TAG = "main";

#define RED   0x001000
#define GREEN 0x100000
#define BLUE  0x000010

#include <stdint.h>
#include <math.h>

#define MAX_BRIGHTNESS 0.1f // 20% brightness

uint32_t get_next_color_full_spectrum(void) {
    static uint32_t hue = 0;
    float r, g, b;

    // Convert hue to RGB
    float h = hue / 65536.0f;
    float x = 1 - fabsf(fmodf(h * 6, 2) - 1);

    if (h < 1.0f/6.0f)      { r = 1; g = x; b = 0; }
    else if (h < 2.0f/6.0f) { r = x; g = 1; b = 0; }
    else if (h < 3.0f/6.0f) { r = 0; g = 1; b = x; }
    else if (h < 4.0f/6.0f) { r = 0; g = x; b = 1; }
    else if (h < 5.0f/6.0f) { r = x; g = 0; b = 1; }
    else                    { r = 1; g = 0; b = x; }

    // Apply brightness limit
    r *= MAX_BRIGHTNESS * 255;
    g *= MAX_BRIGHTNESS * 255;
    b *= MAX_BRIGHTNESS * 255;

    // Increment hue for next call
    hue++;
    if (hue >= 65536) hue = 0;

    // Convert to GRB format
    return ((uint32_t)g << 16) | ((uint32_t)r << 8) | (uint32_t)b;
}

QueueHandle_t gpio_intr_evt_queue = NULL;
void gpio_interrupt_task(void *pvParameters)
{
    uint32_t io_num;
    for(;;) {
        if(xQueueReceive(gpio_intr_evt_queue, &io_num, portMAX_DELAY)) {
            if (gpio_get_level(io_num) == 1) {
                ESP_LOGI(TAG, "GPIO[%lu] interrupt occurred (falling edge)!\n", io_num);

                MotorUpdate update_a = {0, 80, 1};
                xQueueSend(motor_queue[0], &update_a, 0);

                vTaskDelay(50 / portTICK_PERIOD_MS);  // Delay

                // turn off the motor
                update_a.speed_percent = 0;
                xQueueSend(motor_queue[0], &update_a, 0);

            }
        }
    }
}

// this is the main logic loop
void game_logic_loop(void *pvParameters){

    while(1){
        vTaskDelay(1000 / portTICK_PERIOD_MS);  // Delay
    }

}


uint32_t splash_screen[8][15] = {
        {0x200000, 0x200000, 0x200000, 0x200000, 0x200000, 0x200000, 0x200000, 0x200000, 0x200000, 0x200000, 0x200000, 0x0CFF00, 0x0CFF00, 0x200000, 0x200000},
        {0x200000, 0x200000, 0x200000, 0x200000, 0x200000, 0x200000, 0x200000, 0x200000, 0x200000, 0x200000, 0x200000, 0x0CFF00, 0x0CFF00, 0x200000, 0x200000},
        {0x200000, 0x200000, 0x200000, 0x200000, 0x200000, 0x200000, 0x200000, 0x200000, 0x200000, 0x200000, 0x200000, 0x200000, 0x200000, 0x200000, 0x200000},
        {0x200000, 0xE5FF00, 0xFFFFFF, 0x200000, 0x200000, 0x200000, 0x0CFF00, 0x0CFF00, 0x200000, 0x200000, 0x200000, 0x200000, 0x200000, 0x200000, 0x200000},
        {0x200000, 0xE5FF00, 0xFFF000, 0x200000, 0x200000, 0x200000, 0x0CFF00, 0x0CFF00, 0x200000, 0x200000, 0x200000, 0x200000, 0x200000, 0x200000, 0x200000},
        {0x200000, 0x200000, 0x200000, 0x200000, 0x200000, 0x200000, 0x0CFF00, 0x0CFF00, 0x200000, 0x200000, 0x200000, 0x200000, 0x200000, 0x200000, 0x200000},
        {0x200000, 0x200000, 0x200000, 0x200000, 0x200000, 0x200000, 0x0CFF00, 0x0CFF00, 0x200000, 0x200000, 0x200000, 0x200000, 0x200000, 0x200000, 0x0CFF00},
        {0x200000, 0x200000, 0x200000, 0x200000, 0x200000, 0x200000, 0x0CFF00, 0x0CFF00, 0x200000, 0x200000, 0x200000, 0x200000, 0x200000, 0x200000, 0x0CFF00}
};

// Function to limit a color component
uint8_t limit_brightness(uint8_t color, uint8_t max_brightness) {
    return (color * max_brightness) / 255;
}

void app_main(void)
{

    usb_serial_jtag_driver_config_t usb_serial_jtag_config = USB_SERIAL_JTAG_DRIVER_CONFIG_DEFAULT();
    usb_serial_jtag_driver_install(&usb_serial_jtag_config);

    esp_vfs_usb_serial_jtag_use_driver();

    vTaskDelay(1000 / portTICK_PERIOD_MS);  // Delay


    ESP_LOGI(TAG, "starting up");

    // init led driver
    ws2812_control_init();
    struct led_state new_state;
    ESP_LOGI(TAG, "ws driver initialized");

    // BLE Setup -------------------
//    nimble_port_init();
//    ble_hs_cfg.sync_cb = sync_cb;
//    ble_hs_cfg.reset_cb = reset_cb;
//    gatt_svr_init();
//    ble_svc_gap_device_name_set(device_name);
//    nimble_port_freertos_init(host_task);

    // initilize interrupt and input on IO1
    configure_gpio_interrupt();
    gpio_intr_evt_queue = gpio_interrupt_get_evt_queue();
    xTaskCreate(gpio_interrupt_task, "gpio_interrupt_task", 2048, NULL, 10, NULL);

    // haptic feedback
    motor_init();

    // send a test run
    // TODO: reuse the controller from Racer for auto timeouts on haptic feedback
    MotorUpdate update_a = {0, 100, 1};
    xQueueSend(motor_queue[0], &update_a, 0);

    vTaskDelay(100 / portTICK_PERIOD_MS);  // Delay

    // turn off the motor
    update_a.speed_percent = 0;
    xQueueSend(motor_queue[0], &update_a, 0);


    // initialize the task for game logic loop
    xTaskCreate(game_logic_loop, "game_logic_loop", 2048,
                NULL, 5, NULL);


    int led_num = 0;
    // conver the buffer to our screen xy
    for(int i = 0; i < 8; i++){
        for (int j = 0; j < 15; j++){

            // break out the r g b
            uint8_t r = splash_screen[i][j] >> 16;
            uint8_t g = splash_screen[i][j] >> 8;
            uint8_t b = splash_screen[i][j] >> 0;

            if(i % 2 != 0){
                // odd, have to reverse it
                r = splash_screen[i][14 - j] >> 16;
                g = splash_screen[i][14 - j] >> 8;
                b = splash_screen[i][14 - j] >> 0;
            }


            // Apply brightness limit
            r = limit_brightness(r, 30);
            g = limit_brightness(g, 30);
            b = limit_brightness(b, 30);

            // re-arrange rgb to grb and write it to the array
            new_state.leds[led_num] = (((uint32_t)g) << 16) | (((uint32_t)r) << 8) | (((uint32_t)b) << 0);
            led_num++;
        }
    }


    ws2812_write_leds(new_state);


    while (1) {

        for(int i = 0; i < 120; i++)
            new_state.leds[i] = get_next_color_full_spectrum();

        ws2812_write_leds(new_state);

        vTaskDelay(10 / portTICK_PERIOD_MS);  // Delay

    }
}