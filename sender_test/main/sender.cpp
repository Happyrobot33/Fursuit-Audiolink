#include <string.h>
#include "esp_log.h"
#include "esp_now.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "pb_encode.h"
#include "audiolink_data.pb.h"
#include "sender.h"
#include "shared.h"

static const char *TAG = "espnow_sender";
static const uint8_t BROADCAST_MAC[ESP_NOW_ETH_ALEN] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

static volatile bool send_done = true;

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
    send_done = true;
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
    ESP_ERROR_CHECK(esp_now_add_peer(&peer));
}

void espnow_sender_task(void *arg)
{
    ESP_LOGI(TAG, "ESP-NOW sender task started");
    QueuedAudioFrame frame;

    while (true) {
        /* Wait for a frame from the queue (with timeout) */
        if (xQueueReceive(audio_queue, &frame, pdMS_TO_TICKS(1000)) == pdTRUE) {
            /* We have a decoded audio frame, split into Sub_Packets and send */
            
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
                    
                    /* Wait for previous send to complete */
                    while (!send_done) {
                        vTaskDelay(1);
                    }
                    send_done = false;

                    esp_err_t err = esp_now_send(BROADCAST_MAC, pkt_buf, pkt_len);
                    if (err != ESP_OK) {
                        ESP_LOGE(TAG, "esp_now_send failed: %s", esp_err_to_name(err));
                        send_done = true;
                    }
                    
                    ESP_LOGI(TAG, "Sent Sub_Packet %d/%d, chunk_size=%zu, encoded_size=%zu bytes", 
                             packet_index + 1, total_packets, chunk_size, pkt_len);
                    
                    if (chunk_size > 0) {
                        offset += chunk_size;
                    }
                } else {
                    ESP_LOGE(TAG, "Failed to encode Sub_Packet: %s", PB_GET_ERROR(&stream));
                    send_done = true;
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
