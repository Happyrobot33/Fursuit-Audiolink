/*
 * SPDX-FileCopyrightText: 2010-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#include <stdio.h>
#include <string.h>
#include <math.h>
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
#include "audiolink_data.pb.h"

static const char *TAG = "audiolink";

// GPIO assignment
#define LED_STRIP_BLINK_GPIO  GPIO_NUM_13
// Number of LEDs in the strip
#define LED_STRIP_LED_NUMBERS 60
// 10MHz resolution, 1 tick = 0.1us
#define LED_STRIP_RMT_RES_HZ  (10 * 1000 * 1000)

/* Buffer for receiving Sub_Packets and reconstructing Audiolink_Data */
#define MAX_SUB_PACKETS 16
#define MAX_AUDIO_DATA_SIZE 10000

typedef struct {
    Sub_Packet packets[MAX_SUB_PACKETS];
    int packet_count;
    bool is_complete;
    uint8_t reconstructed_data[MAX_AUDIO_DATA_SIZE];
    size_t reconstructed_size;
    Audiolink_Data audio_data;
} AudioBuffer;

static AudioBuffer current_audio = {0};
static SemaphoreHandle_t audio_mutex;

/* Track LED pixel state (RGB values) */
typedef struct {
    uint8_t r, g, b;
} pixel_t;
static pixel_t led_pixels[LED_STRIP_LED_NUMBERS] = {0};

/* HSV to RGB conversion - maps highmid value to a color gradient */
static void hsv_to_rgb(float hue, float saturation, float value, uint8_t *r, uint8_t *g, uint8_t *b)
{
    float c = value * saturation;
    float x = c * (1 - fabs(fmod(hue / 60.0, 2) - 1));
    float m = value - c;
    float rf, gf, bf;
    
    if (hue < 60) {
        rf = c; gf = x; bf = 0;
    } else if (hue < 120) {
        rf = x; gf = c; bf = 0;
    } else if (hue < 180) {
        rf = 0; gf = c; bf = x;
    } else if (hue < 240) {
        rf = 0; gf = x; bf = c;
    } else if (hue < 300) {
        rf = x; gf = 0; bf = c;
    } else {
        rf = c; gf = 0; bf = x;
    }
    
    *r = (uint8_t)((rf + m) * 255);
    *g = (uint8_t)((gf + m) * 255);
    *b = (uint8_t)((bf + m) * 255);
}

/* Map frequency values to LED quadrant with specified color
   Maps frequency array to one of 4 equal quadrants of the LED strip
   quadrant: 0-3 (0=bass/red, 1=lowmid/yellow, 2=highmid/green, 3=treble/blue) */
static int map_to_leds(led_strip_handle_t led_strip, float *frequency_value, int frequency_count, 
                       uint8_t quadrant, uint8_t r, uint8_t g, uint8_t b)
{
    if (frequency_count == 0 || quadrant >= 4) {
        return ESP_FAIL;
    }
    
    int quadrant_size = LED_STRIP_LED_NUMBERS / 4;
    int start_led = quadrant * quadrant_size;
    int end_led = start_led + quadrant_size;
    
    for (int i = start_led; i < end_led; i++) {
        /* Map position within quadrant to frequency index */
        int quadrant_pos = i - start_led;
        size_t idx = (quadrant_pos * frequency_count) / quadrant_size;
        if (idx >= frequency_count) {
            idx = frequency_count - 1;
        }
        
        float value = frequency_value[idx];
        if (value > 1.0) value = 1.0;
        if (value < 0.0) value = 0.0;
        
        /* Scale the color by the frequency value (brightness modulation) */
        uint8_t r_scaled = (uint8_t)(r * value);
        uint8_t g_scaled = (uint8_t)(g * value);
        uint8_t b_scaled = (uint8_t)(b * value);
        
        /* Update local pixel state and LED */
        led_pixels[i].r = r_scaled;
        led_pixels[i].g = g_scaled;
        led_pixels[i].b = b_scaled;
        
        ESP_ERROR_CHECK(led_strip_set_pixel(led_strip, i, r_scaled, g_scaled, b_scaled));
    }
    
    return ESP_OK;
}

/* Attempt to reconstruct Audiolink_Data from received Sub_Packets */
static bool try_reconstruct_audio_data(AudioBuffer *buf)
{
    if (buf->packet_count == 0) {
        return false;
    }
    
    /* Concatenate all sub-packet data in order */
    buf->reconstructed_size = 0;
    for (int i = 0; i < buf->packet_count; i++) {
        Sub_Packet *pkt = &buf->packets[i];
        
        if (buf->reconstructed_size + pkt->data_count > MAX_AUDIO_DATA_SIZE) {
            ESP_LOGW(TAG, "Audio data too large, discarding");
            return false;
        }
        
        memcpy(&buf->reconstructed_data[buf->reconstructed_size],
               pkt->data, pkt->data_count);
        buf->reconstructed_size += pkt->data_count;
    }
    
    /* Decode the concatenated data as Audiolink_Data */
    int decode_len = audiolink_data_decode(buf->reconstructed_data,
                                          buf->reconstructed_size,
                                          &buf->audio_data);
    
    if (decode_len > 0) {
        buf->is_complete = true;
        ESP_LOGI(TAG, "Audio data reconstructed: bass=%d, lowmid=%d, highmid=%d, treble=%d",
                 buf->audio_data.history.bass_count,
                 buf->audio_data.history.lowmid_count,
                 buf->audio_data.history.highmid_count,
                 buf->audio_data.history.treble_count);
        return true;
    }
    
    return false;
}

static void espnow_recv_cb(const esp_now_recv_info_t *recv_info, const uint8_t *data, int len)
{
    Sub_Packet pkt;
    sub_packet_init(&pkt);
    
    int decode_len = sub_packet_decode(data, (size_t)len, &pkt);
    if (decode_len <= 0) {
        ESP_LOGW(TAG, "Failed to decode Sub_Packet");
        return;
    }
    
    /* Use debug logging to avoid blocking in callback */
    ESP_LOGD(TAG, "Packet received - MAC: %02x:%02x:%02x:%02x:%02x:%02x, Size: %d bytes, Index: %d, Data size: %d",
             recv_info->src_addr[0], recv_info->src_addr[1], recv_info->src_addr[2],
             recv_info->src_addr[3], recv_info->src_addr[4], recv_info->src_addr[5],
             len, pkt.packet_index, pkt.data_count);
    
    if (xSemaphoreTake(audio_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        /* Check if this is a new message (packet_index == 0) or continuation */
        if (pkt.packet_index == 0) {
            /* New message: reset the buffer */
            current_audio.packet_count = 0;
            current_audio.is_complete = false;
            ESP_LOGD(TAG, "Starting new audio message");
        }
        
        /* Store the packet if we have space */
        if (current_audio.packet_count < MAX_SUB_PACKETS) {
            memcpy(&current_audio.packets[current_audio.packet_count],
                   &pkt, sizeof(Sub_Packet));
            current_audio.packet_count++;
            
            ESP_LOGD(TAG, "Received Sub_Packet %d (total packets in buffer: %d)",
                     pkt.packet_index, current_audio.packet_count);
            
            /* Try to reconstruct - if we have a reasonable number of packets */
            if (current_audio.packet_count >= 1) {
                try_reconstruct_audio_data(&current_audio);
            }
        } else {
            ESP_LOGW(TAG, "Too many Sub_Packets, discarding");
        }
        
        xSemaphoreGive(audio_mutex);
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
    audio_mutex = xSemaphoreCreateMutex();

    gpio_set_drive_capability(LED_STRIP_BLINK_GPIO, GPIO_DRIVE_CAP_3);
    led_strip_handle_t led_strip = configure_led();

    wifi_init();
    ESP_ERROR_CHECK(esp_now_init());
    ESP_ERROR_CHECK(esp_now_register_recv_cb(espnow_recv_cb));
    ESP_LOGI(TAG, "ESP-NOW receiver ready (broadcast) - ESP-NOW v2 mode with 1000+ byte support");

    while (1) {
        if (xSemaphoreTake(audio_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
            if (current_audio.is_complete) {
                /* Clear pixel state and strip */
                for (int i = 0; i < LED_STRIP_LED_NUMBERS; i++) {
                    led_pixels[i].r = 0;
                    led_pixels[i].g = 0;
                    led_pixels[i].b = 0;
                    ESP_ERROR_CHECK(led_strip_set_pixel(led_strip, i, 0, 0, 0));
                }
                
                /* Map 4 frequency bands to 4 quadrants */
                /* Quadrant 0: bass -> red */
                map_to_leds(led_strip, 
                           current_audio.audio_data.history.bass, 
                           current_audio.audio_data.history.bass_count,
                           0, 255, 0, 0);
                
                /* Quadrant 1: lowmid -> yellow */
                map_to_leds(led_strip, 
                           current_audio.audio_data.history.lowmid,
                           current_audio.audio_data.history.lowmid_count,
                           1, 255, 255, 0);
                
                /* Quadrant 2: highmid -> green */
                map_to_leds(led_strip, 
                           current_audio.audio_data.history.highmid,
                           current_audio.audio_data.history.highmid_count,
                           2, 0, 255, 0);
                
                /* Quadrant 3: treble -> blue */
                map_to_leds(led_strip, 
                           current_audio.audio_data.history.treble,
                           current_audio.audio_data.history.treble_count,
                           3, 0, 0, 255);
                
                ESP_ERROR_CHECK(led_strip_refresh(led_strip));
                
                /* Reset flag after displaying to avoid redundant updates */
                current_audio.is_complete = false;
            }
            xSemaphoreGive(audio_mutex);
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}
