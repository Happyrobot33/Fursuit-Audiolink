#include <string.h>
#include <atomic>
#include "esp_log.h"
#include "esp_now.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "pb_encode.h"
#include "audiolink_data.pb.h"
#include "sender.h"
#include "shared.h"

static const char *TAG = "espnow_sender";
static const uint8_t BROADCAST_MAC[ESP_NOW_ETH_ALEN] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
static constexpr uint32_t MAX_IN_FLIGHT_SENDS = 24;

static std::atomic<uint32_t> send_callbacks{0};
static std::atomic<uint32_t> send_requests{0};

/* Callback for encoding the repeated bytes field */
typedef struct {
    const uint8_t *data;
    size_t size;
} BytesEncoderContext;

static bool encode_data_callback(pb_ostream_t *stream, const pb_field_t *field, void * const *arg)
{
    BytesEncoderContext *ctx = (BytesEncoderContext *)*arg;
    
    if (!pb_encode_tag_for_field(stream, field)) {
        return false;
    }
    
    return pb_encode_string(stream, ctx->data, ctx->size);
}

static void on_data_sent(const esp_now_send_info_t *tx_info, esp_now_send_status_t status)
{
    (void)tx_info;
    (void)status;
    send_callbacks.fetch_add(1, std::memory_order_relaxed);
}

void espnow_init(void)
{
    ESP_ERROR_CHECK(esp_now_init());
    ESP_ERROR_CHECK(esp_now_register_send_cb(on_data_sent));

    esp_now_peer_info_t peer{};
    peer.channel = 0;
    peer.ifidx   = WIFI_IF_STA;
    peer.encrypt = false;
    memcpy(peer.peer_addr, BROADCAST_MAC, ESP_NOW_ETH_ALEN);
    // esp_now_rate_config_t rate_config{};
    // rate_config.phymode = WIFI_PHY_MODE_11A;
    // rate_config.rate    = WIFI_PHY_RATE_54M;
    // rate_config.ersu    = false;
    // rate_config.dcm     = false;
    ESP_ERROR_CHECK(esp_now_add_peer(&peer));
    // ESP_ERROR_CHECK(esp_now_set_peer_rate_config(peer.peer_addr, &rate_config));
    esp_wifi_config_80211_tx_rate(WIFI_IF_STA, WIFI_PHY_RATE_54M);
}

void espnow_sender_task(void *arg)
{
    ESP_LOGI(TAG, "ESP-NOW sender task started");
    QueuedAudioFrame frame;

    uint32_t fps_frames = 0;
    uint32_t fps_packets = 0;
    size_t   fps_bytes = 0;
    uint64_t last_fps_log_us = esp_timer_get_time();

    while (true) {
        /* Wait for a frame from the queue (with timeout) */
        if (xQueueReceive(audio_queue, &frame, pdMS_TO_TICKS(10)) == pdTRUE) {
            fps_frames++;
            fps_bytes += frame.data_len;

            /* Calculate total number of packets for this frame */
            int total_packets = (frame.data_len > 0) 
                ? (frame.data_len + SUB_PACKET_DATA_COUNT - 1) / SUB_PACKET_DATA_COUNT
                : 1;  /* At least one packet even if data is empty */
            
            int packet_index = 0;
            size_t offset = 0;

            while (offset < frame.data_len || packet_index == 0) {
                PROTO_Sub_Packet sub_pkt = PROTO_Sub_Packet_init_zero;
                sub_pkt.packet_index = packet_index;
                sub_pkt.packet_count = total_packets;

                /* Calculate chunk size */
                size_t chunk_size = frame.data_len - offset;
                if (chunk_size > SUB_PACKET_DATA_COUNT) {
                    chunk_size = SUB_PACKET_DATA_COUNT;
                }

                /* Set up callback for data field */
                BytesEncoderContext data_ctx = {
                    .data = (chunk_size > 0) ? &frame.data[offset] : NULL,
                    .size = chunk_size
                };
                sub_pkt.data.funcs.encode = encode_data_callback;
                sub_pkt.data.arg = &data_ctx;

                /* Encode Sub_Packet into buffer */
                uint8_t pkt_buf[ESP_NOW_MAX_DATA_LEN_V2];
                pb_ostream_t stream = pb_ostream_from_buffer(pkt_buf, sizeof(pkt_buf));
                bool encode_status = pb_encode(&stream, PROTO_Sub_Packet_fields, &sub_pkt);
                
                if (encode_status) {
                    size_t pkt_len = stream.bytes_written;

                    while ((send_requests.load(std::memory_order_relaxed) -
                            send_callbacks.load(std::memory_order_relaxed)) >= MAX_IN_FLIGHT_SENDS) {
                        vTaskDelay(1);
                    }

                    esp_err_t err = esp_now_send(BROADCAST_MAC, pkt_buf, pkt_len);
                    if (err != ESP_OK) {
                        ESP_LOGE(TAG, "esp_now_send failed: %s", esp_err_to_name(err));
                    } else {
                        send_requests.fetch_add(1, std::memory_order_relaxed);
                        fps_packets++;
                    }
                    
                    if (chunk_size > 0) {
                        offset += chunk_size;
                    }
                } else {
                    ESP_LOGE(TAG, "Failed to encode Sub_Packet: %s", PB_GET_ERROR(&stream));
                    break;
                }

                packet_index++;
                if (offset >= frame.data_len) {
                    break;
                }
            }
        }

        /* Log framerate every second */
        uint64_t now_us = esp_timer_get_time();
        if (now_us - last_fps_log_us >= 1000000ULL) {
            float elapsed_sec = (float)(now_us - last_fps_log_us) / 1000000.0f;
            float fps = (float)fps_frames / elapsed_sec;
            float pps = (float)fps_packets / elapsed_sec;
            float kbps = (float)(fps_bytes * 8) / (elapsed_sec * 1000.0f);

            ESP_LOGI(TAG, "Framerate: %.1f FPS | Packets: %.1f PPS | Bitrate: %.1f kbps",
                     fps, pps, kbps);

            fps_frames = 0;
            fps_packets = 0;
            fps_bytes = 0;
            last_fps_log_us = now_us;
        }
    }
}
