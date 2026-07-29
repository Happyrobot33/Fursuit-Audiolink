#include <stdio.h>
#include <string.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "nvs_flash.h"
#include "esp_wifi.h"
#include "esp_now.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "driver/uart.h"
#include "audiolink_data.pb.h"

static const char *TAG = "espnow_sender";

static const uint8_t BROADCAST_MAC[ESP_NOW_ETH_ALEN] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

// ---- UART config --------------------------------------------------------
// Using UART0 (the built-in USB connection on the ESP32)
#define SERIAL_UART_NUM     UART_NUM_0
#define SERIAL_UART_RX_PIN  3
#define SERIAL_UART_TX_PIN  1
// #define SERIAL_BAUD_RATE    115200
#define SERIAL_BAUD_RATE    921600  // Higher baud rate for faster throughput

// ESP-NOW max payload is ESP_NOW_MAX_DATA_LEN_V2 bytes (250+ in v2 mode)
#define COBS_MAX_ENC  4096   /* encoded frame buffer (includes potential 0x00 delimiter) */
#define COBS_MAX_DEC  4000   /* decoded payload buffer */

/* Queue message for passing decoded audio data to sender task */
typedef struct {
    uint8_t data[COBS_MAX_DEC];
    size_t data_len;
} QueuedAudioFrame;

static QueueHandle_t audio_queue = NULL;

// ---- COBS decoder -------------------------------------------------------
// Returns decoded length on success, -1 on encoding error.

static int cobs_decode(const uint8_t *enc, size_t enc_len,
                       uint8_t *dec, size_t dec_max)
{
    size_t in = 0, out = 0;

    while (in < enc_len) {
        uint8_t code = enc[in++];
        if (code == 0x00) {
            return -1; /* 0x00 inside encoded data is invalid */
        }
        /* Copy (code - 1) literal bytes */
        for (uint8_t i = 1; i < code; i++) {
            if (in >= enc_len || out >= dec_max) return -1;
            dec[out++] = enc[in++];
        }
        /* Append an implicit 0x00 unless this is the final block (code == 0xFF) */
        if (code != 0xFF && in < enc_len) {
            if (out >= dec_max) return -1;
            dec[out++] = 0x00;
        }
    }
    return (int)out;
}

// -------------------------------------------------------------------------

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

// ---- Test mode: generate test data without UART ---------------------------
// For testing without UART, generate arbitrary Audiolink_Data and broadcast via ESP-NOW.

#define TEST_MODE 0  // Set to 0 to use UART, 1 to use test mode

// ---- ESP-NOW Sender Task (decoupled from UART reading) -------------------
// Pulls decoded frames from a queue and sends via ESP-NOW
// This task can block without affecting UART throughput

static void espnow_sender_task(void *arg)
{
    ESP_LOGI(TAG, "ESP-NOW sender task started");
    QueuedAudioFrame frame;

    while (true) {
        /* Wait for a frame from the queue (with timeout) */
        if (xQueueReceive(audio_queue, &frame, pdMS_TO_TICKS(1000)) == pdTRUE) {
            /* We have a decoded audio frame, split into Sub_Packets and send */
            int packet_index = 0;
            size_t offset = 0;

            while (offset < frame.data_len || packet_index == 0) {
                Sub_Packet sub_pkt;
                sub_packet_init(&sub_pkt);
                sub_pkt.packet_index = packet_index;

                /* Calculate chunk size */
                size_t chunk_size = frame.data_len - offset;
                if (chunk_size > SUB_PACKET_DATA_COUNT) {
                    chunk_size = SUB_PACKET_DATA_COUNT;
                }

                if (chunk_size > 0) {
                    memcpy(sub_pkt.data, &frame.data[offset], chunk_size);
                    sub_pkt.data_count = chunk_size;
                    offset += chunk_size;
                }

                /* Encode and send Sub_Packet */
                uint8_t pkt_buf[ESP_NOW_MAX_DATA_LEN_V2];
                int pkt_len = sub_packet_encode(pkt_buf, sizeof(pkt_buf), &sub_pkt);

                if (pkt_len > 0) {
                    /* Wait for previous send to complete */
                    while (!send_done) {
                        vTaskDelay(1);
                    }
                    send_done = false;

                    esp_err_t err = esp_now_send(BROADCAST_MAC, pkt_buf, (size_t)pkt_len);
                    if (err != ESP_OK) {
                        ESP_LOGE(TAG, "esp_now_send failed: %s", esp_err_to_name(err));
                        send_done = true;
                    }
                } else {
                    ESP_LOGE(TAG, "Sub_Packet encoding failed");
                    break;
                }

                packet_index++;
                if (offset >= frame.data_len) {
                    break;
                }
            }
        }
    }
}

// ---- UART Serial Receive Task (interrupt-driven, non-blocking) -----------

#if !TEST_MODE
static void serial_rx_task(void *arg)
{
    const uart_config_t uart_cfg = {
        .baud_rate  = SERIAL_BAUD_RATE,
        .data_bits  = UART_DATA_8_BITS,
        .parity     = UART_PARITY_DISABLE,
        .stop_bits  = UART_STOP_BITS_1,
        .flow_ctrl  = UART_HW_FLOWCTRL_DISABLE,
    };
    ESP_ERROR_CHECK(uart_param_config(SERIAL_UART_NUM, &uart_cfg));
    ESP_ERROR_CHECK(uart_set_pin(SERIAL_UART_NUM,
                                 SERIAL_UART_TX_PIN, SERIAL_UART_RX_PIN,
                                 UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    
    /* Declare queue handle for UART interrupts */
    QueueHandle_t uart_queue;
    
    /* Install UART driver with event queue (interrupt-driven) */
    /* Ring buffer sized for 3 packets max to prevent stale packet accumulation */
    ESP_ERROR_CHECK(uart_driver_install(SERIAL_UART_NUM, COBS_MAX_ENC * 3, 0, 10, &uart_queue, 0));

    ESP_LOGI(TAG, "UART initialized (interrupt-driven): num=%d, baud=%d, rx_pin=%d, tx_pin=%d, buffer=%d bytes",
             SERIAL_UART_NUM, SERIAL_BAUD_RATE, SERIAL_UART_RX_PIN, SERIAL_UART_TX_PIN, COBS_MAX_ENC * 3);

    uint8_t enc_buf[COBS_MAX_ENC];
    uint8_t dec_buf[COBS_MAX_DEC];
    uint8_t read_buf[512];  /* Chunk buffer for efficient reading */
    int     enc_len = 0;
    uart_event_t event;

    while (true) {
        /* Wait for UART events (blocks with proper task yield) */
        if (xQueueReceive(uart_queue, (void *)&event, portMAX_DELAY) != pdTRUE) {
            continue;
        }

        switch (event.type) {
            case UART_DATA:
                /* Data available in UART RX buffer */
                if (event.size > 0) {
                    size_t read_size = (event.size > sizeof(read_buf)) ? sizeof(read_buf) : event.size;
                    int n = uart_read_bytes(SERIAL_UART_NUM, read_buf, read_size, 0);
                    
                    /* Process the chunk byte by byte */
                    for (int i = 0; i < n; i++) {
                        uint8_t byte = read_buf[i];
                        ESP_LOGD(TAG, "UART RX byte: 0x%02x (%c)", byte, (byte >= 0x20 && byte < 0x7F) ? byte : '.');

                        if (byte == 0x00) {
                            /* End-of-frame delimiter: decode and queue Audiolink_Data */
                            if (enc_len > 0) {
                                ESP_LOGI(TAG, "Frame delimiter received, encoded frame size: %d bytes", enc_len);
                                int dec_len = cobs_decode(enc_buf, (size_t)enc_len, dec_buf, sizeof(dec_buf));
                                if (dec_len > 0) {
                                    /* Decode the Audiolink_Data message to extract info and verify */
                                    Audiolink_Data audio_data;
                                    audiolink_data_init(&audio_data);

                                    int audio_len = audiolink_data_decode(dec_buf, (size_t)dec_len, &audio_data);
                                    
                                    if (audio_len > 0) {
                                        ESP_LOGI(TAG, "Received Audiolink_Data: bass=%d, lowmid=%d, highmid=%d, treble=%d",
                                                 audio_data.history.bass_count,
                                                 audio_data.history.lowmid_count,
                                                 audio_data.history.highmid_count,
                                                 audio_data.history.treble_count);
                                        
                                        /* Print first float from each band */
                                        if (audio_data.history.bass_count > 0) {
                                            ESP_LOGI(TAG, "Bass[0] = %f", audio_data.history.bass[0]);
                                        }
                                        if (audio_data.history.lowmid_count > 0) {
                                            ESP_LOGI(TAG, "LowMid[0] = %f", audio_data.history.lowmid[0]);
                                        }
                                        if (audio_data.history.highmid_count > 0) {
                                            ESP_LOGI(TAG, "HighMid[0] = %f", audio_data.history.highmid[0]);
                                        }
                                        if (audio_data.history.treble_count > 0) {
                                            ESP_LOGI(TAG, "Treble[0] = %f", audio_data.history.treble[0]);
                                        }
                                        
                                        /* Queue the decoded frame for the sender task */
                                        QueuedAudioFrame queued_frame;
                                        queued_frame.data_len = (size_t)dec_len;
                                        memcpy(queued_frame.data, dec_buf, (size_t)dec_len);
                                        
                                        if (xQueueSend(audio_queue, &queued_frame, 0) != pdTRUE) {
                                            ESP_LOGW(TAG, "Audio queue full, frame dropped");
                                        }
                                    } else {
                                        ESP_LOGW(TAG, "Audiolink_Data decode failed");
                                    }
                                } else {
                                    ESP_LOGW(TAG, "COBS decode error");
                                }
                            }
                            enc_len = 0; /* reset for next frame */
                        } else {
                            /* Accumulate encoded bytes; drop oversized frames */
                            if (enc_len < COBS_MAX_ENC) {
                                enc_buf[enc_len++] = byte;
                            } else {
                                ESP_LOGW(TAG, "Frame too large, discarding");
                                enc_len = 0;
                            }
                        }
                    }  /* End of for loop processing read_buf chunk */
                }
                break;

            case UART_FIFO_OVF:
                ESP_LOGW(TAG, "UART RX FIFO overflow");
                uart_flush_input(SERIAL_UART_NUM);
                enc_len = 0;
                break;

            case UART_BUFFER_FULL:
                ESP_LOGW(TAG, "UART RX buffer full");
                uart_flush_input(SERIAL_UART_NUM);
                enc_len = 0;
                break;

            case UART_BREAK:
                ESP_LOGD(TAG, "UART RX break signal");
                break;

            case UART_PARITY_ERR:
                ESP_LOGW(TAG, "UART parity error");
                break;

            case UART_FRAME_ERR:
                ESP_LOGW(TAG, "UART frame error");
                break;

            default:
                ESP_LOGD(TAG, "Unknown UART event: %d", event.type);
                break;
        }
    }
}
#endif  // !TEST_MODE

// ---- Test mode task: generate and broadcast arbitrary audiolink data -----
#if TEST_MODE
static void test_data_task(void *arg)
{
    ESP_LOGI(TAG, "Test mode: Generating sin wave audiolink data");

    /* Persistent buffers for each frequency band (128 elements max) */
    static float bass_buffer[128] = {0};
    static float lowmid_buffer[128] = {0};
    static float highmid_buffer[128] = {0};
    static float treble_buffer[128] = {0};
    static int buffer_count = 0;

    while (true) {
        /* Create a test Audiolink_Data message with sample values */
        Audiolink_Data audio_data;
        audiolink_data_init(&audio_data);

        /* Generate sin wave value based on microcontroller time (1-second period) */
        int64_t time_us = esp_timer_get_time();
        float sin_value = sinf(2.0f * M_PI * (float)(time_us % 10000000) / 10000000.0f);  /* Full cycle every 1 second */
        sin_value = (sin_value + 1.0f) / 2.0f;  /* Map from [-1, 1] to [0, 1] */

        /* Shift existing values down and insert new sin value at the start */
        for (int i = 127; i > 0; i--) {
            bass_buffer[i] = bass_buffer[i - 1];
            lowmid_buffer[i] = lowmid_buffer[i - 1];
            highmid_buffer[i] = highmid_buffer[i - 1];
            treble_buffer[i] = treble_buffer[i - 1];
        }

        /* Insert new sin value at position 0 for each band */
        bass_buffer[0] = sin_value;
        lowmid_buffer[0] = sin_value;
        highmid_buffer[0] = sin_value;
        treble_buffer[0] = sin_value;

        /* Update buffer count (max 128) */
        if (buffer_count < 128) {
            buffer_count++;
        }

        /* Copy buffers to audio_data (only buffer_count elements) */
        for (int i = 0; i < buffer_count; i++) {
            if (audio_data.history.bass_count < HISTORY_BASS_COUNT) {
                audio_data.history.bass[audio_data.history.bass_count++] = bass_buffer[i];
            }
            if (audio_data.history.lowmid_count < HISTORY_LOWMID_COUNT) {
                audio_data.history.lowmid[audio_data.history.lowmid_count++] = lowmid_buffer[i];
            }
            if (audio_data.history.highmid_count < HISTORY_HIGHMID_COUNT) {
                audio_data.history.highmid[audio_data.history.highmid_count++] = highmid_buffer[i];
            }
            if (audio_data.history.treble_count < HISTORY_TREBLE_COUNT) {
                audio_data.history.treble[audio_data.history.treble_count++] = treble_buffer[i];
            }
        }

        /* Encode the Audiolink_Data message */
        uint8_t *dec_buf = (uint8_t *)malloc(COBS_MAX_DEC);
        if (!dec_buf) {
            ESP_LOGE(TAG, "Failed to allocate decode buffer");
            vTaskDelay(pdMS_TO_TICKS(2000));
            continue;
        }
        
        int dec_len = audiolink_data_encode(dec_buf, COBS_MAX_DEC, &audio_data);

        if (dec_len > 0) {
            ESP_LOGI(TAG, "Test: Encoded Audiolink_Data: bass=%d, lowmid=%d, highmid=%d, treble=%d, encoded_len=%d",
                     audio_data.history.bass_count,
                     audio_data.history.lowmid_count,
                     audio_data.history.highmid_count,
                     audio_data.history.treble_count,
                     dec_len);
        } else {
            ESP_LOGE(TAG, "Test: Audiolink_Data encoding failed (dec_len=%d)", dec_len);
            free(dec_buf);
            vTaskDelay(pdMS_TO_TICKS(2000));
            continue;
        }

        if (dec_len > 0) {
            int packet_index = 0;
            size_t offset = 0;

            while (offset < (size_t)dec_len || packet_index == 0) {
                Sub_Packet sub_pkt;
                sub_packet_init(&sub_pkt);
                sub_pkt.packet_index = packet_index;

                /* Calculate chunk size for this sub-packet */
                size_t chunk_size = (size_t)dec_len - offset;
                if (chunk_size > SUB_PACKET_DATA_COUNT) {
                    chunk_size = SUB_PACKET_DATA_COUNT;
                }

                if (chunk_size > 0) {
                    memcpy(sub_pkt.data, &dec_buf[offset], chunk_size);
                    sub_pkt.data_count = chunk_size;
                    offset += chunk_size;
                }

                /* Encode and send the Sub_Packet via ESP-NOW */
                uint8_t *pkt_buf = (uint8_t *)malloc(ESP_NOW_MAX_DATA_LEN_V2);
                if (!pkt_buf) {
                    ESP_LOGE(TAG, "Failed to allocate packet buffer");
                    free(dec_buf);
                    break;
                }
                
                int pkt_len = sub_packet_encode(pkt_buf, ESP_NOW_MAX_DATA_LEN_V2, &sub_pkt);

                if (pkt_len > 0) {
                    /* Wait for any previous send to complete */
                    while (!send_done) {
                        vTaskDelay(1);
                    }
                    send_done = false;

                    esp_err_t err = esp_now_send(BROADCAST_MAC, pkt_buf, (size_t)pkt_len);
                    if (err != ESP_OK) {
                        ESP_LOGE(TAG, "esp_now_send failed: %s", esp_err_to_name(err));
                        send_done = true;
                    } else {
                        ESP_LOGI(TAG, "Test: Sent Sub_Packet index=%d len=%d", packet_index, pkt_len);
                    }
                } else {
                    ESP_LOGE(TAG, "Sub_Packet encoding failed");
                    free(pkt_buf);
                    break;
                }

                free(pkt_buf);
                packet_index++;
                if (offset >= (size_t)dec_len) {
                    break;
                }
            }
        }
        
        free(dec_buf);

        /* Minimal delay to yield to other tasks */
        vTaskDelay(1);
    }
}
#endif  // TEST_MODE


// -------------------------------------------------------------------------

void app_main(void)
{
    ESP_ERROR_CHECK(nvs_flash_init());

    wifi_init();
    espnow_init();

    /* Create queue for passing decoded audio frames to sender task */
    audio_queue = xQueueCreate(10, sizeof(QueuedAudioFrame));
    if (audio_queue == NULL) {
        ESP_LOGE(TAG, "Failed to create audio queue");
        return;
    }

#if TEST_MODE
    ESP_LOGI(TAG, "Starting in TEST MODE - generating arbitrary audiolink data");
    xTaskCreate(test_data_task, "test_data", 16384, NULL, 5, NULL);
#else
    ESP_LOGI(TAG, "Starting in UART MODE - reading audiolink data from serial");
    /* Create sender task (higher priority to ensure fast ESP-NOW transmission) */
    xTaskCreate(espnow_sender_task, "espnow_sender", 16384, NULL, 6, NULL);
    /* Create UART reader task (lower priority, won't be blocked by sending) */
    xTaskCreate(serial_rx_task, "serial_rx", 16384, NULL, 5, NULL);
#endif
}
