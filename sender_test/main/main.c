#include <stdio.h>
#include <string.h>
#include <math.h>
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "esp_wifi.h"
#include "esp_now.h"
#include "esp_log.h"

static const char *TAG = "espnow_sender";

static const uint8_t BROADCAST_MAC[ESP_NOW_ETH_ALEN] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

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

static volatile bool send_done = true;

static void on_data_sent(const esp_now_send_info_t *tx_info, esp_now_send_status_t status)
{
    send_done = true;
}

static void wifi_init(void)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());
}

static void espnow_init(void)
{
    ESP_ERROR_CHECK(esp_now_init());
    ESP_ERROR_CHECK(esp_now_register_send_cb(on_data_sent));

    esp_now_peer_info_t peer = {
        .channel = 0,
        .ifidx   = WIFI_IF_STA,
        .encrypt = false,
    };
    memcpy(peer.peer_addr, BROADCAST_MAC, ESP_NOW_ETH_ALEN);
    ESP_ERROR_CHECK(esp_now_add_peer(&peer));
}

void app_main(void)
{
    ESP_ERROR_CHECK(nvs_flash_init());

    wifi_init();
    espnow_init();

    rgb_packet_t pkt = {
        .count = RGB_PACKET_LEDS,
    };

    while (true) {
        float t = esp_timer_get_time() / 1e6f; // microseconds → seconds
        for (int i = 0; i < RGB_PACKET_LEDS; i++) {
            float offset = (float)i / RGB_PACKET_LEDS; // phase offset per LED [0, 1)
            uint8_t r = (uint8_t)(25.0f + 25.0f * sinf(2.0f * M_PI * (t - offset)));
            pkt.leds[i] = (rgb_t){.r = r, .g = 0, .b = 0};
        }

        while (!send_done) {
            vTaskDelay(1);
        }
        send_done = false;

        esp_err_t err = esp_now_send(BROADCAST_MAC, (uint8_t *)&pkt, sizeof(pkt));
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "esp_now_send failed: %s", esp_err_to_name(err));
            send_done = true; // unblock on error so we don't stall
        }
    }
}
