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
#include "pb_decode.h"

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
#define MAX_FREQUENCY_SAMPLES 256

/* Decoded frequency data arrays */
typedef struct {
    float bass[MAX_FREQUENCY_SAMPLES];
    size_t bass_count;
    float lowmid[MAX_FREQUENCY_SAMPLES];
    size_t lowmid_count;
    float highmid[MAX_FREQUENCY_SAMPLES];
    size_t highmid_count;
    float treble[MAX_FREQUENCY_SAMPLES];
    size_t treble_count;
} FrequencyData;

typedef struct {
    uint32_t packet_index;
    uint8_t data[ESP_NOW_MAX_DATA_LEN_V2];
    size_t data_len;
} PacketBuffer;

typedef struct {
    PacketBuffer packets[MAX_SUB_PACKETS];
    int packet_count;
    bool is_complete;
    uint8_t reconstructed_data[MAX_AUDIO_DATA_SIZE];
    size_t reconstructed_size;
    Audiolink_Data audio_data;
    FrequencyData frequency_data;
} AudioBuffer;

static AudioBuffer current_audio = {0};
static SemaphoreHandle_t audio_mutex;

/* Track LED pixel state (RGB values) */
typedef struct {
    uint8_t r, g, b;
} pixel_t;
static pixel_t led_pixels[LED_STRIP_LED_NUMBERS] = {0};

/* Structure to hold decoded Sub_Packet data in callback */
typedef struct {
    uint8_t buffer[ESP_NOW_MAX_DATA_LEN_V2];
    size_t size;
} SubPacketData;

/* Callback handler for decoding repeated bytes data field */
static bool decode_subpacket_data_callback(pb_istream_t *stream, const pb_field_t *field, void **arg)
{
    SubPacketData *pkt_data = (SubPacketData *)*arg;
    
    /* For a repeated bytes field, stream->bytes_left contains the length of this element */
    size_t bytes_to_read = stream->bytes_left;
    
    if (pkt_data->size + bytes_to_read > ESP_NOW_MAX_DATA_LEN_V2) {
        ESP_LOGW(TAG, "Sub_Packet data too large: %zu + %zu > %zu", 
                 pkt_data->size, bytes_to_read, (size_t)ESP_NOW_MAX_DATA_LEN_V2);
        return false;
    }
    
    if (!pb_read(stream, &pkt_data->buffer[pkt_data->size], bytes_to_read)) {
        return false;
    }
    
    pkt_data->size += bytes_to_read;
    return true;
}

/* Callback handlers for decoding repeated float fields */
static bool decode_bass_callback(pb_istream_t *stream, const pb_field_t *field, void **arg)
{
    FrequencyData *freq_data = (FrequencyData *)*arg;
    float value;
    if (!pb_decode_fixed32(stream, (uint32_t*)&value)) {
        return false;
    }
    if (freq_data->bass_count < MAX_FREQUENCY_SAMPLES) {
        freq_data->bass[freq_data->bass_count++] = value;
    }
    return true;
}

static bool decode_lowmid_callback(pb_istream_t *stream, const pb_field_t *field, void **arg)
{
    FrequencyData *freq_data = (FrequencyData *)*arg;
    float value;
    if (!pb_decode_fixed32(stream, (uint32_t*)&value)) {
        return false;
    }
    if (freq_data->lowmid_count < MAX_FREQUENCY_SAMPLES) {
        freq_data->lowmid[freq_data->lowmid_count++] = value;
    }
    return true;
}

static bool decode_highmid_callback(pb_istream_t *stream, const pb_field_t *field, void **arg)
{
    FrequencyData *freq_data = (FrequencyData *)*arg;
    float value;
    if (!pb_decode_fixed32(stream, (uint32_t*)&value)) {
        return false;
    }
    if (freq_data->highmid_count < MAX_FREQUENCY_SAMPLES) {
        freq_data->highmid[freq_data->highmid_count++] = value;
    }
    return true;
}

static bool decode_treble_callback(pb_istream_t *stream, const pb_field_t *field, void **arg)
{
    FrequencyData *freq_data = (FrequencyData *)*arg;
    float value;
    if (!pb_decode_fixed32(stream, (uint32_t*)&value)) {
        return false;
    }
    if (freq_data->treble_count < MAX_FREQUENCY_SAMPLES) {
        freq_data->treble[freq_data->treble_count++] = value;
    }
    return true;
}

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

static bool try_reconstruct_audio_data(AudioBuffer *buf)
{
    if (buf->packet_count == 0) {
        return false;
    }
    
    /* Assume all packets have the same packet_count from the first packet */
    uint32_t expected_packet_count = buf->packets[0].packet_index; // Will be set in recv callback
    
    /* Check if we have all expected packets */
    if (buf->packet_count < expected_packet_count) {
        ESP_LOGD(TAG, "Waiting for packets: %d/%d", buf->packet_count, expected_packet_count);
        return false;
    }
    
    /* Concatenate all packet data in order */
    buf->reconstructed_size = 0;
    for (int i = 0; i < buf->packet_count; i++) {
        PacketBuffer *pkt = &buf->packets[i];
        
        if (buf->reconstructed_size + pkt->data_len > MAX_AUDIO_DATA_SIZE) {
            ESP_LOGW(TAG, "Audio data too large, discarding");
            return false;
        }
        
        if (pkt->data_len > 0) {
            memcpy(&buf->reconstructed_data[buf->reconstructed_size],
                   pkt->data, pkt->data_len);
            buf->reconstructed_size += pkt->data_len;
        }
    }
    
    /* Initialize frequency data structure */
    memset(&buf->frequency_data, 0, sizeof(FrequencyData));
    
    /* Set up callbacks for repeated float fields */
    buf->audio_data.history.bass.funcs.decode = decode_bass_callback;
    buf->audio_data.history.bass.arg = &buf->frequency_data;
    
    buf->audio_data.history.lowmid.funcs.decode = decode_lowmid_callback;
    buf->audio_data.history.lowmid.arg = &buf->frequency_data;
    
    buf->audio_data.history.highmid.funcs.decode = decode_highmid_callback;
    buf->audio_data.history.highmid.arg = &buf->frequency_data;
    
    buf->audio_data.history.treble.funcs.decode = decode_treble_callback;
    buf->audio_data.history.treble.arg = &buf->frequency_data;
    
    /* Decode the concatenated data as Audiolink_Data using nanopb */
    pb_istream_t stream = pb_istream_from_buffer(buf->reconstructed_data, buf->reconstructed_size);
    bool status = pb_decode(&stream, Audiolink_Data_fields, &buf->audio_data);
    
    if (status) {
        buf->is_complete = true;
        ESP_LOGI(TAG, "Audio data reconstructed: received %d packets, bass=%zu, lowmid=%zu, highmid=%zu, treble=%zu",
                 buf->packet_count,
                 buf->frequency_data.bass_count,
                 buf->frequency_data.lowmid_count,
                 buf->frequency_data.highmid_count,
                 buf->frequency_data.treble_count);
        return true;
    } else {
        ESP_LOGW(TAG, "Protobuf decode failed: %s", PB_GET_ERROR(&stream));
        return false;
    }
}

static void espnow_recv_cb(const esp_now_recv_info_t *recv_info, const uint8_t *data, int len)
{
    /* Decode the received data as a Sub_Packet protobuf message */
    Sub_Packet sub_pkt = {0};
    SubPacketData pkt_data = {0};
    
    /* Set up callback for data field */
    sub_pkt.data.funcs.decode = decode_subpacket_data_callback;
    sub_pkt.data.arg = &pkt_data;
    
    pb_istream_t stream = pb_istream_from_buffer(data, len);
    bool decode_status = pb_decode(&stream, Sub_Packet_fields, &sub_pkt);
    
    if (!decode_status) {
        ESP_LOGW(TAG, "Failed to decode Sub_Packet: %s", PB_GET_ERROR(&stream));
        return;
    }
    
    /* Use debug logging to avoid blocking in callback */
    ESP_LOGD(TAG, "Packet received - MAC: %02x:%02x:%02x:%02x:%02x:%02x, Index: %d, Count: %d, Data size: %d",
             recv_info->src_addr[0], recv_info->src_addr[1], recv_info->src_addr[2],
             recv_info->src_addr[3], recv_info->src_addr[4], recv_info->src_addr[5],
             sub_pkt.packet_index, sub_pkt.packet_count, pkt_data.size);
    
    if (xSemaphoreTake(audio_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        /* Check if this is a new message (packet_index == 0) or continuation */
        if (sub_pkt.packet_index == 0) {
            /* New message: reset the buffer */
            current_audio.packet_count = 0;
            current_audio.is_complete = false;
            ESP_LOGD(TAG, "Starting new audio message (expecting %d packets)", sub_pkt.packet_count);
        }
        
        /* Store the packet if we have space */
        if (current_audio.packet_count < MAX_SUB_PACKETS) {
            PacketBuffer *pkt = &current_audio.packets[current_audio.packet_count];
            pkt->packet_index = sub_pkt.packet_index;
            pkt->data_len = pkt_data.size;
            memcpy(pkt->data, pkt_data.buffer, pkt_data.size);
            current_audio.packet_count++;
            
            ESP_LOGD(TAG, "Received packet %d/%d (total buffered: %d)",
                     sub_pkt.packet_index, sub_pkt.packet_count, current_audio.packet_count);
            
            /* Try to reconstruct - if we have all expected packets */
            if (current_audio.packet_count >= sub_pkt.packet_count) {
                try_reconstruct_audio_data(&current_audio);
            }
        } else {
            ESP_LOGW(TAG, "Too many packets, discarding");
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
                           current_audio.frequency_data.bass, 
                           current_audio.frequency_data.bass_count,
                           0, 255, 0, 0);
                
                /* Quadrant 1: lowmid -> yellow */
                map_to_leds(led_strip, 
                           current_audio.frequency_data.lowmid,
                           current_audio.frequency_data.lowmid_count,
                           1, 255, 255, 0);
                
                /* Quadrant 2: highmid -> green */
                map_to_leds(led_strip, 
                           current_audio.frequency_data.highmid,
                           current_audio.frequency_data.highmid_count,
                           2, 0, 255, 0);
                
                /* Quadrant 3: treble -> blue */
                map_to_leds(led_strip, 
                           current_audio.frequency_data.treble,
                           current_audio.frequency_data.treble_count,
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
