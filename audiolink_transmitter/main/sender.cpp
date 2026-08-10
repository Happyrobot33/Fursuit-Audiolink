#include <string.h>
#include <vector>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "pb_encode.h"
#include "audiolink_data.pb.h"
#include "sender.h"
#include "shared.h"
#include "RF24.h"
#include "driver/spi_master.h"

static const char *TAG = "sender";

static constexpr uint8_t RF24_CHANNEL = 90;
static constexpr rf24_datarate_e RF24_DATA_RATE = RF24_250KBPS;
static constexpr rf24_pa_dbm_e RF24_PA_LEVEL = RF24_PA_LOW;
static constexpr uint8_t RF24_PIPE_ADDR[5] = {'A', 'L', 'N', 'K', '1'};

#define CE_PIN GPIO_NUM_32
#define CSN_PIN GPIO_NUM_33
#define MOSI_PIN GPIO_NUM_23
#define MISO_PIN GPIO_NUM_19
#define SCK_PIN GPIO_NUM_18
#define IRQ_PIN GPIO_NUM_36

RF24 radio = RF24(CE_PIN, CSN_PIN);
static SPIClass radio_spi;

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

static bool encode_sub_packet_to_buffer(
    uint8_t *buf,
    size_t buf_size,
    int32_t packet_index,
    int32_t packet_count,
    const uint8_t *data,
    size_t data_size,
    size_t *encoded_len)
{
    PROTO_Sub_Packet sub_pkt = PROTO_Sub_Packet_init_zero;
    sub_pkt.packet_index = packet_index;
    sub_pkt.packet_count = packet_count;

    BytesEncoderContext data_ctx = {
        .data = (data_size > 0) ? data : NULL,
        .size = data_size
    };
    sub_pkt.data.funcs.encode = encode_data_callback;
    sub_pkt.data.arg = &data_ctx;

    pb_ostream_t stream = pb_ostream_from_buffer(buf, buf_size);
    if (!pb_encode(&stream, PROTO_Sub_Packet_fields, &sub_pkt)) {
        return false;
    }

    *encoded_len = stream.bytes_written;
    return true;
}

static size_t find_max_chunk_size_for_payload(
    size_t frame_len,
    size_t payload_size,
    const uint8_t *probe_data)
{
    if (frame_len == 0) {
        return 0;
    }

    if (payload_size == 0) {
        return 0;
    }

    size_t low = 1;
    size_t high = (frame_len < SUB_PACKET_DATA_COUNT) ? frame_len : SUB_PACKET_DATA_COUNT;
    size_t best = 0;
    std::vector<uint8_t> probe_buf(payload_size);

    // Binary-search the largest data chunk that still fits after protobuf encoding.
    while (low <= high) {
        size_t mid = low + (high - low) / 2;
        int32_t total_packets = (int32_t)((frame_len + mid - 1) / mid);
        size_t encoded_len = 0;

        bool fits = encode_sub_packet_to_buffer(
            probe_buf.data(),
            probe_buf.size(),
            total_packets - 1,
            total_packets,
            probe_data,
            mid,
            &encoded_len);

        if (fits && encoded_len <= payload_size) {
            best = mid;
            low = mid + 1;
        } else {
            if (mid == 0) {
                break;
            }
            high = mid - 1;
        }
    }

    return best;
}

void setup_radio()
{
    radio_spi.begin(SPI2_HOST);
    // to use the custom defined pins, uncomment the following
    // hspi->begin(MY_SCLK, MY_MISO, MY_MOSI, MY_SS)
    ESP_LOGI(TAG, "SPI bus configured");

    while (!radio.begin(&radio_spi)) {
        ESP_LOGE(TAG, "Radio hardware not responding!");
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }

    radio.maskIRQ(1, 1, 0);
    radio.setAddressWidth(5);
    radio.setCRCLength(RF24_CRC_16);
    radio.setPALevel(RF24_PA_LEVEL);
    radio.setDataRate(RF24_DATA_RATE);
    radio.setChannel(RF24_CHANNEL);
    radio.setAutoAck(false);
    radio.setPayloadSize(32);
    radio.setRetries(0, 0);
    radio.flush_tx();
    radio.openWritingPipe(RF24_PIPE_ADDR);
    radio.stopListening();
    
    ESP_LOGI(TAG,
             "Radio initialized (pipe=ALNK1, channel=%u, rate=250kbps, payload=%u)",
             static_cast<unsigned>(RF24_CHANNEL),
             static_cast<unsigned>(radio.getPayloadSize()));
}

void sender_task(void *arg)
{
    ESP_LOGI(TAG, "ESP-NOW sender task started");
    QueuedAudioFrame frame;

    while (true) {
        //just send dummy data
        static const char *dummy_msg = "Hello";
        radio.write((const uint8_t *)dummy_msg, strlen(dummy_msg));
        //wait for 1 second
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }

    while (true) {
        /* Wait for a frame from the queue (with timeout) */
        if (xQueueReceive(audio_queue, &frame, pdMS_TO_TICKS(10)) == pdTRUE) {
            /* We have a decoded audio frame, split into Sub_Packets and send */

            const size_t payload_size = radio.getPayloadSize();
            const size_t max_chunk_size = find_max_chunk_size_for_payload(frame.data_len, payload_size, frame.data);

            if (frame.data_len > 0 && max_chunk_size == 0) {
                ESP_LOGE(TAG, "Frame dropped: no Sub_Packet can fit in payload size=%zu", payload_size);
                continue;
            }

            /* Calculate total number of packets for this frame based on fit-tested chunk size */
            int total_packets = (frame.data_len > 0)
                ? (int)((frame.data_len + max_chunk_size - 1) / max_chunk_size)
                : 1;  /* At least one packet even if data is empty */

            int packet_index = 0;
            size_t offset = 0;

            std::vector<uint8_t> pkt_buf(payload_size);

            while (offset < frame.data_len || packet_index == 0) {
                /* Calculate chunk size */
                size_t chunk_size = frame.data_len - offset;
                if (chunk_size > max_chunk_size) {
                    chunk_size = max_chunk_size;
                }

                /* Encode Sub_Packet into buffer */
                size_t pkt_len = 0;
                bool encode_status = encode_sub_packet_to_buffer(
                    pkt_buf.data(),
                    pkt_buf.size(),
                    packet_index,
                    total_packets,
                    (chunk_size > 0) ? &frame.data[offset] : NULL,
                    chunk_size,
                    &pkt_len);
                
                if (encode_status) {
                    const bool tx_ok = radio.write(pkt_buf.data(), static_cast<uint8_t>(pkt_len));
                    if (!tx_ok) {
                        ESP_LOGW(TAG,
                                 "RF24 write failed for Sub_Packet %d/%d",
                                 packet_index + 1,
                                 total_packets);
                    }
                    
                    ESP_LOGI(TAG, "Sent Sub_Packet %d/%d, chunk_size=%zu, encoded_size=%zu bytes", 
                             packet_index + 1, total_packets, chunk_size, pkt_len);
                    
                    if (chunk_size > 0) {
                        offset += chunk_size;
                    }
                } else {
                    ESP_LOGE(TAG,
                             "Failed to encode Sub_Packet %d/%d (chunk_size=%zu, payload_size=%zu)",
                             packet_index + 1,
                             total_packets,
                             chunk_size,
                             payload_size);
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
