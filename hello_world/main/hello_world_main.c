/*
 * SPDX-FileCopyrightText: 2010-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#include <stdio.h>
#include <string.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_now.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "nvs_flash.h"
#include "driver/gpio.h"
#include "led_strip_types.h"
#include "led_strip_rmt.h"
#include "led_strip.h"

static const char *TAG = "audiolink";

// GPIO assignment
#define LED_STRIP_BLINK_GPIO  GPIO_NUM_13
// Number of LEDs in the strip
#define LED_STRIP_LED_NUMBERS 60
// 10MHz resolution, 1 tick = 0.1us
#define LED_STRIP_RMT_RES_HZ  (10 * 1000 * 1000)

#define RGB_PACKET_LEDS 10

typedef struct {
    uint8_t r;
    uint8_t g;
    uint8_t b;
} __attribute__((packed)) rgb_t;

typedef struct {
    uint8_t count;
    rgb_t   leds[RGB_PACKET_LEDS];
} __attribute__((packed)) rgb_packet_t;

static rgb_packet_t current_packet = {0};
static SemaphoreHandle_t color_mutex;

static void espnow_recv_cb(const esp_now_recv_info_t *recv_info, const uint8_t *data, int len)
{
    if (len != sizeof(rgb_packet_t)) {
        ESP_LOGW(TAG, "Unexpected packet size: %d (expected %d)", len, sizeof(rgb_packet_t));
        return;
    }
    if (xSemaphoreTake(color_mutex, 0) == pdTRUE) {
        memcpy(&current_packet, data, sizeof(rgb_packet_t));
        xSemaphoreGive(color_mutex);
    }
}

static void wifi_init(void)
{
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());
}

static led_strip_handle_t configure_led(void)
{
    led_strip_config_t strip_config = {
        .strip_gpio_num = LED_STRIP_BLINK_GPIO,
        .max_leds = LED_STRIP_LED_NUMBERS,
        .color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_GRB,
        .led_model = LED_MODEL_WS2812,
        .flags.invert_out = false,
    };
    led_strip_rmt_config_t rmt_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = LED_STRIP_RMT_RES_HZ,
        .flags.with_dma = false,
    };
    led_strip_handle_t led_strip;
    ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip));
    ESP_LOGI(TAG, "Created LED strip object with RMT backend");
    return led_strip;
}

void app_main(void)
{
    color_mutex = xSemaphoreCreateMutex();

    gpio_set_drive_capability(LED_STRIP_BLINK_GPIO, GPIO_DRIVE_CAP_3);
    led_strip_handle_t led_strip = configure_led();

    wifi_init();
    ESP_ERROR_CHECK(esp_now_init());
    ESP_ERROR_CHECK(esp_now_register_recv_cb(espnow_recv_cb));
    ESP_LOGI(TAG, "ESP-NOW receiver ready (broadcast)");

    while (1) {
        rgb_packet_t pkt;
        xSemaphoreTake(color_mutex, portMAX_DELAY);
        pkt = current_packet;
        xSemaphoreGive(color_mutex);

        uint8_t count = pkt.count > 0 ? pkt.count : RGB_PACKET_LEDS;
        for (int i = 0; i < LED_STRIP_LED_NUMBERS; i++) {
            rgb_t c = pkt.leds[i % count];
            ESP_ERROR_CHECK(led_strip_set_pixel(led_strip, i, c.r, c.g, c.b));
        }
        ESP_ERROR_CHECK(led_strip_refresh(led_strip));
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}
