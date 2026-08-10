#include <algorithm>
#include <cstring>
#include <vector>
#include <zlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "audiolink_data.pb.h"
#include "pb_decode.h"
#include "nanopb_cpp.h"
#include "config.h"
#include "receiver.h"
#include "audio_converters.hpp"
#include "RF24.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"

#define CE_PIN GPIO_NUM_32
#define CSN_PIN GPIO_NUM_33
#define MOSI_PIN GPIO_NUM_23
#define MISO_PIN GPIO_NUM_19
#define SCK_PIN GPIO_NUM_18
#define IRQ_PIN GPIO_NUM_36

RF24 radio = RF24(CE_PIN, CSN_PIN);
static SPIClass radio_spi;
static bool radio_initialized = false;
static volatile bool radio_irq_pending = false;
static uint32_t subpacket_decode_failures = 0;

static constexpr uint8_t RF24_CHANNEL = 90;
static constexpr rf24_datarate_e RF24_DATA_RATE = RF24_250KBPS;
static constexpr rf24_pa_dbm_e RF24_PA_LEVEL = RF24_PA_LOW;
static constexpr uint8_t RF24_PIPE_ADDR[5] = {'A', 'L', 'N', 'K', '1'};

using namespace NanoPb::Converter;

// Local working buffers for packet reconstruction
static std::vector<uint8_t> reconstructed_data;
static std::vector<std::vector<uint8_t>> packet_chunks;
static int packet_count = 0;
static int expected_packet_count = 0;
static TaskHandle_t process_task_handle = nullptr;

constexpr size_t FRAME_QUEUE_DEPTH = 2;
constexpr size_t MAX_RF24_PACKETS_PER_POLL = 8;

struct CompressedFrame {
    std::vector<uint8_t> *data;
    int packet_count;
};

struct DecodedSubPacket {
    int32_t packet_index;
    int32_t packet_count;
    std::vector<uint8_t> data;
};

static QueueHandle_t free_compressed_queue = nullptr;
static QueueHandle_t ready_compressed_queue = nullptr;
static QueueHandle_t free_decoded_queue = nullptr;
static QueueHandle_t ready_decoded_queue = nullptr;
static bool pipeline_initialized = false;

static std::vector<uint8_t> compressed_frame_pool[FRAME_QUEUE_DEPTH];
static AudiolinkData decoded_audio_pool[FRAME_QUEUE_DEPTH];

static void IRAM_ATTR receiver_rf24_irq_isr(void *arg) {
    (void)arg;
    radio_irq_pending = true;

    if (process_task_handle) {
        BaseType_t higher_priority_task_woken = pdFALSE;
        vTaskNotifyGiveFromISR(process_task_handle, &higher_priority_task_woken);
        if (higher_priority_task_woken == pdTRUE) {
            portYIELD_FROM_ISR();
        }
    }
}

static bool receiver_init_pipeline_queues(void) {
    if (pipeline_initialized) {
        return true;
    }

    free_compressed_queue = xQueueCreate(FRAME_QUEUE_DEPTH, sizeof(std::vector<uint8_t> *));
    ready_compressed_queue = xQueueCreate(FRAME_QUEUE_DEPTH, sizeof(CompressedFrame));
    free_decoded_queue = xQueueCreate(FRAME_QUEUE_DEPTH, sizeof(AudiolinkData *));
    ready_decoded_queue = xQueueCreate(FRAME_QUEUE_DEPTH, sizeof(AudiolinkData *));

    if (!free_compressed_queue || !ready_compressed_queue || !free_decoded_queue || !ready_decoded_queue) {
        ESP_LOGE(TAG, "Failed to create receiver pipeline queues");
        return false;
    }

    for (size_t i = 0; i < FRAME_QUEUE_DEPTH; ++i) {
        std::vector<uint8_t> *compressed_slot = &compressed_frame_pool[i];
        AudiolinkData *decoded_slot = &decoded_audio_pool[i];
        (void)xQueueSend(free_compressed_queue, &compressed_slot, 0);
        (void)xQueueSend(free_decoded_queue, &decoded_slot, 0);
    }

    pipeline_initialized = true;
    return true;
}

static bool decode_subpacket_data_callback(pb_istream_t *stream, const pb_field_t *field, void **arg) {
    std::vector<uint8_t> *buffer = static_cast<std::vector<uint8_t>*>(*arg);
    size_t bytes_to_read = stream->bytes_left;
    
    size_t old_size = buffer->size();
    buffer->resize(old_size + bytes_to_read);
    
    if (!pb_read(stream, buffer->data() + old_size, bytes_to_read)) {
        return false;
    }
    
    return true;
}

static bool decode_subpacket_from_buffer(const uint8_t *payload,
                                         size_t payload_len,
                                         DecodedSubPacket &decoded_out) {
    if (!payload || payload_len == 0) {
        return false;
    }

    auto try_decode_len = [&](size_t candidate_len) -> bool {
        PROTO_Sub_Packet sub_pkt = PROTO_Sub_Packet_init_zero;
        std::vector<uint8_t> pkt_data;

        sub_pkt.data.funcs.decode = decode_subpacket_data_callback;
        sub_pkt.data.arg = &pkt_data;

        pb_istream_t stream = pb_istream_from_buffer(payload, candidate_len);
        bool decode_ok = pb_decode(&stream, PROTO_Sub_Packet_fields, &sub_pkt);

        if (!decode_ok) {
            return false;
        }

        decoded_out.packet_index = sub_pkt.packet_index;
        decoded_out.packet_count = sub_pkt.packet_count;
        decoded_out.data = std::move(pkt_data);
        return true;
    };

    // Fixed-width RF24 payloads are commonly zero-padded; try the full length
    // and a trimmed length first before scanning remaining candidates.
    if (try_decode_len(payload_len)) {
        return true;
    }

    size_t trimmed_len = payload_len;
    while (trimmed_len > 0 && payload[trimmed_len - 1] == 0) {
        --trimmed_len;
    }

    if (trimmed_len > 0 && trimmed_len != payload_len && try_decode_len(trimmed_len)) {
        return true;
    }

    for (size_t candidate_len = trimmed_len; candidate_len >= 1; --candidate_len) {
        if (candidate_len == payload_len || candidate_len == trimmed_len) {
            continue;
        }

        if (try_decode_len(candidate_len)) {
            return true;
        }
    }

    return false;
}

static bool payload_is_all_zero(const uint8_t *payload, size_t payload_len) {
    if (!payload || payload_len == 0) {
        return true;
    }

    return std::all_of(payload, payload + payload_len, [](uint8_t byte) {
        return byte == 0;
    });
}

static bool decode_streaming_zlib_payload(const std::vector<uint8_t> &compressed_data,
                                          AudiolinkData &decoded_audio,
                                          size_t *decompressed_size_out) {
    ESP_LOGD(TAG, "Streaming inflate payload: compressed size=%zu bytes", compressed_data.size());
    if (compressed_data.empty()) {
        return false;
    }

    z_stream zstream = {};
    zstream.next_in = const_cast<Bytef *>(compressed_data.data());
    zstream.avail_in = static_cast<uInt>(compressed_data.size());

    if (inflateInit2(&zstream, -15) != Z_OK) {
        ESP_LOGW(TAG, "Raw DEFLATE inflate initialization failed");
        return false;
    }

    static uint8_t decompressed_data[MAX_AUDIO_DATA_SIZE];
    size_t decompressed_size = 0;

    int zlib_result = Z_OK;
    do {
        zstream.next_out = decompressed_data + decompressed_size;
        zstream.avail_out = static_cast<uInt>(MAX_AUDIO_DATA_SIZE - decompressed_size);
        zlib_result = inflate(&zstream, Z_NO_FLUSH);

        if (zlib_result != Z_OK && zlib_result != Z_STREAM_END) {
            ESP_LOGW(TAG, "ZLIB decompression failed with code %d", zlib_result);
            inflateEnd(&zstream);
            return false;
        }

        size_t produced = MAX_AUDIO_DATA_SIZE - decompressed_size - zstream.avail_out;
        decompressed_size += produced;

        if (zlib_result == Z_OK && zstream.avail_out == 0) {
            ESP_LOGW(TAG, "Decompressed payload exceeds maximum size limit");
            inflateEnd(&zstream);
            return false;
        }
    } while (zlib_result != Z_STREAM_END);

    inflateEnd(&zstream);

    pb_istream_t pb_stream = pb_istream_from_buffer(decompressed_data,
                                                    decompressed_size);

    bool decode_ok = NanoPb::decode<AudiolinkDataConverter>(pb_stream, decoded_audio);

    if (!decode_ok) {
        ESP_LOGW(TAG, "Protobuf decode failed: %s", PB_GET_ERROR(&pb_stream));
    }

    if (!decode_ok) {
        return false;
    }

    if (decompressed_size_out) {
        *decompressed_size_out = decompressed_size;
    }

    return true;
}

static bool try_reconstruct_audio_data(const std::vector<uint8_t> &compressed_data,
                                       int received_packet_count,
                                       AudiolinkData &decoded_out) {
    if (compressed_data.empty()) {
        return false;
    }

    ESP_LOGD(TAG, "Attempting to reconstruct audio data from %zu bytes of compressed data", compressed_data.size());

    /* Pooled decode slots are reused; reset vectors before callback-driven append decode. */
    decoded_out = AudiolinkData{};

    size_t decompressed_size = 0;
    if (decode_streaming_zlib_payload(compressed_data, decoded_out, &decompressed_size)) {
        ESP_LOGD(TAG, "Audio data reconstructed: received %d packets, compressed size=%zu bytes, decompressed size=%zu bytes",
                 received_packet_count,
                 compressed_data.size(),
                 decompressed_size);
        return true;
    } else {
        ESP_LOGW(TAG, "Unable to stream-decode reconstructed payload");
        return false;
    }
}

static void receiver_handle_radio_payload(const uint8_t *data, size_t len) {
    if (!pipeline_initialized) {
        return;
    }

    if (payload_is_all_zero(data, len)) {
        return;
    }

    DecodedSubPacket sub_pkt;
    if (!decode_subpacket_from_buffer(data, len, sub_pkt)) {
        ++subpacket_decode_failures;
        ESP_LOGW(TAG, "Failed to decode Sub_Packet from RF24 payload (%zu bytes)", len);
        if ((subpacket_decode_failures % 16U) == 1U && len > 0) {
            const size_t preview_len = std::min<size_t>(len, 16);
            ESP_LOG_BUFFER_HEX_LEVEL(TAG, data, preview_len, ESP_LOG_WARN);
        }
        return;
    }

    ESP_LOGI(TAG, "RF24 packet received - Index: %ld, Count: %ld, Data size: %zu",
             static_cast<long>(sub_pkt.packet_index),
             static_cast<long>(sub_pkt.packet_count),
             sub_pkt.data.size());

    if (sub_pkt.packet_count == 1 &&
        sub_pkt.packet_index == 0 &&
        sub_pkt.data.size() == 1 &&
        sub_pkt.data[0] == 0xAA) {
        ESP_LOGI(TAG, "RF probe packet received (radio link is alive)");
        return;
    }

    if (sub_pkt.packet_count <= 0 || sub_pkt.packet_count > MAX_SUB_PACKETS) {
        ESP_LOGW(TAG, "Ignoring packet with invalid packet_count %d", sub_pkt.packet_count);
        return;
    }

    if (sub_pkt.packet_index < 0 || sub_pkt.packet_index >= sub_pkt.packet_count) {
        ESP_LOGW(TAG, "Ignoring packet with invalid index %d for count %d",
                 sub_pkt.packet_index,
                 sub_pkt.packet_count);
        return;
    }
    
    /* Check if this is a new message (packet_index == 0) or continuation */
    if (sub_pkt.packet_index == 0) {
        /* New message: reset the buffer */
        reconstructed_data.clear();
        packet_chunks.clear();
        packet_count = 0;
        expected_packet_count = sub_pkt.packet_count;
        packet_chunks.assign(expected_packet_count, std::vector<uint8_t>());
        ESP_LOGD(TAG, "Starting new audio message (expecting %d packets)", expected_packet_count);
    }

    if (expected_packet_count > 0 && sub_pkt.packet_index < expected_packet_count) {
        if (packet_chunks[sub_pkt.packet_index].empty()) {
            packet_chunks[sub_pkt.packet_index] = std::move(sub_pkt.data);
            packet_count++;
        } else {
            ESP_LOGD(TAG, "Duplicate packet index %d ignored", sub_pkt.packet_index);
        }

        ESP_LOGD(TAG, "Received packet %d/%d (stored: %d/%d)",
                 sub_pkt.packet_index,
                 expected_packet_count,
                 packet_count,
                 expected_packet_count);

        /* Try to reconstruct once all expected packets are present */
        if (packet_count >= expected_packet_count) {
            reconstructed_data.clear();
            bool can_reconstruct = true;

            for (int i = 0; i < expected_packet_count; ++i) {
                const std::vector<uint8_t> &chunk = packet_chunks[i];
                if (chunk.empty()) {
                    can_reconstruct = false;
                    break;
                }

                if (reconstructed_data.size() + chunk.size() > MAX_AUDIO_DATA_SIZE) {
                    ESP_LOGW(TAG, "Audio data too large, discarding");
                    can_reconstruct = false;
                    break;
                }

                reconstructed_data.insert(reconstructed_data.end(), chunk.begin(), chunk.end());
            }

            if (can_reconstruct) {
                std::vector<uint8_t> *compressed_slot = nullptr;
                if (xQueueReceive(free_compressed_queue, &compressed_slot, 0) != pdTRUE) {
                    CompressedFrame dropped_frame;
                    if (xQueueReceive(ready_compressed_queue, &dropped_frame, 0) == pdTRUE) {
                        dropped_frame.data->clear();
                        (void)xQueueSend(free_compressed_queue, &dropped_frame.data, 0);
                    }
                    (void)xQueueReceive(free_compressed_queue, &compressed_slot, 0);
                }

                if (compressed_slot) {
                    compressed_slot->assign(reconstructed_data.begin(), reconstructed_data.end());

                    CompressedFrame ready_frame = {
                        .data = compressed_slot,
                        .packet_count = packet_count,
                    };

                    if (xQueueSend(ready_compressed_queue, &ready_frame, 0) != pdTRUE) {
                        compressed_slot->clear();
                        (void)xQueueSend(free_compressed_queue, &compressed_slot, 0);
                    }

                    if (process_task_handle) {
                        xTaskNotifyGive(process_task_handle);
                    }
                } else {
                    ESP_LOGW(TAG, "No free compressed frame slot available");
                }

                reconstructed_data.clear();
                packet_chunks.clear();
                packet_count = 0;
                expected_packet_count = 0;
            }
        }
    } else {
        if (expected_packet_count == 0) {
            ESP_LOGD(TAG, "No active frame; dropping non-start packet index %d", sub_pkt.packet_index);
        } else {
            ESP_LOGW(TAG, "Ignoring packet with out-of-range index %d (expected %d)",
                     sub_pkt.packet_index,
                     expected_packet_count);
        }
    }
}

void receiver_process_pending(void) {
    if (!pipeline_initialized) {
        return;
    }

    CompressedFrame compressed_frame;
    if (xQueueReceive(ready_compressed_queue, &compressed_frame, 0) != pdTRUE) {
        return;
    }

    AudiolinkData *decoded_slot = nullptr;
    if (xQueueReceive(free_decoded_queue, &decoded_slot, 0) != pdTRUE) {
        AudiolinkData *dropped_decoded = nullptr;
        if (xQueueReceive(ready_decoded_queue, &dropped_decoded, 0) == pdTRUE) {
            (void)xQueueSend(free_decoded_queue, &dropped_decoded, 0);
        }
        (void)xQueueReceive(free_decoded_queue, &decoded_slot, 0);
    }

    if (!decoded_slot) {
        compressed_frame.data->clear();
        (void)xQueueSend(free_compressed_queue, &compressed_frame.data, 0);
        ESP_LOGW(TAG, "No free decoded frame slot available");
        return;
    }

    bool decode_ok = try_reconstruct_audio_data(*compressed_frame.data,
                                                compressed_frame.packet_count,
                                                *decoded_slot);

    if (decode_ok) {
        if (xQueueSend(ready_decoded_queue, &decoded_slot, 0) != pdTRUE) {
            AudiolinkData *dropped_decoded = nullptr;
            if (xQueueReceive(ready_decoded_queue, &dropped_decoded, 0) == pdTRUE) {
                (void)xQueueSend(free_decoded_queue, &dropped_decoded, 0);
                (void)xQueueSend(ready_decoded_queue, &decoded_slot, 0);
            } else {
                (void)xQueueSend(free_decoded_queue, &decoded_slot, 0);
            }
        }
    } else {
        (void)xQueueSend(free_decoded_queue, &decoded_slot, 0);
    }

    compressed_frame.data->clear();
    (void)xQueueSend(free_compressed_queue, &compressed_frame.data, 0);
}

void receiver_set_process_task_handle(TaskHandle_t task_handle) {
    (void)receiver_init_pipeline_queues();
    process_task_handle = task_handle;
}

bool receiver_take_decoded_frame(AudiolinkData &out_audio, TickType_t wait_ticks) {
    if (!pipeline_initialized) {
        return false;
    }

    AudiolinkData *decoded_slot = nullptr;
    if (xQueueReceive(ready_decoded_queue, &decoded_slot, wait_ticks) != pdTRUE || !decoded_slot) {
        return false;
    }

    out_audio = *decoded_slot;
    (void)xQueueSend(free_decoded_queue, &decoded_slot, 0);
    return true;
}

void receiver_rf24_init(void) {
    ESP_ERROR_CHECK_WITHOUT_ABORT(receiver_init_pipeline_queues() ? ESP_OK : ESP_FAIL);

    gpio_config_t irq_gpio_cfg = {};
    irq_gpio_cfg.pin_bit_mask = (1ULL << IRQ_PIN);
    irq_gpio_cfg.mode = GPIO_MODE_INPUT;
    irq_gpio_cfg.pull_up_en = GPIO_PULLUP_DISABLE;
    irq_gpio_cfg.pull_down_en = GPIO_PULLDOWN_DISABLE;
    irq_gpio_cfg.intr_type = GPIO_INTR_NEGEDGE;

    ESP_ERROR_CHECK(gpio_config(&irq_gpio_cfg));
    esp_err_t isr_install_err = gpio_install_isr_service(ESP_INTR_FLAG_IRAM);
    if (isr_install_err != ESP_OK && isr_install_err != ESP_ERR_INVALID_STATE) {
        ESP_ERROR_CHECK(isr_install_err);
    }
    ESP_ERROR_CHECK(gpio_isr_handler_add(IRQ_PIN, receiver_rf24_irq_isr, nullptr));
    ESP_ERROR_CHECK(gpio_intr_enable(IRQ_PIN));
    ESP_LOGI(TAG,
             "RF24 IRQ configured on GPIO %d (initial level=%d)",
             static_cast<int>(IRQ_PIN),
             gpio_get_level(IRQ_PIN));

    radio_spi.begin(SPI2_HOST);
    ESP_LOGI(TAG, "RF24 SPI bus configured");

    while (!radio.begin(&radio_spi)) {
        ESP_LOGE(TAG, "RF24 radio hardware not responding");
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    radio.setStatusFlags(RF24_RX_DR);
    (void)radio.clearStatusFlags(RF24_IRQ_ALL);
    radio.setAddressWidth(5);
    radio.setCRCLength(RF24_CRC_16);
    radio.setPALevel(RF24_PA_LEVEL);
    radio.setDataRate(RF24_DATA_RATE);
    radio.setChannel(RF24_CHANNEL);
    radio.setAutoAck(false);
    radio.setPayloadSize(32);
    radio.setRetries(0, 0);

    radio.flush_rx();
    radio.openReadingPipe(0, RF24_PIPE_ADDR);
    radio.openReadingPipe(1, RF24_PIPE_ADDR);
    radio.startListening();
    (void)radio.clearStatusFlags(RF24_IRQ_ALL);
    ESP_LOGI(TAG,
             "RF24 listening started (IRQ level=%d)",
             gpio_get_level(IRQ_PIN));
    radio_irq_pending = false;

    radio_initialized = true;
    ESP_LOGI(TAG,
             "RF24 receiver ready (pipe=ALNK1, channel=%u, rate=250kbps, payload=%u)",
             static_cast<unsigned>(RF24_CHANNEL),
             static_cast<unsigned>(radio.getPayloadSize()));
}

void receiver_rf24_poll(void) {
    if (!radio_initialized || !pipeline_initialized) {
        return;
    }

    if (!radio_irq_pending) {
        return;
    }

    radio_irq_pending = false;
    const uint8_t status = radio.clearStatusFlags(RF24_IRQ_ALL);
    if ((status & RF24_RX_DR) == 0) {
        return;
    }

    size_t packets_processed = 0;
    while (radio.available() && packets_processed < MAX_RF24_PACKETS_PER_POLL) {
        uint8_t payload_buf[32] = {0};
        const uint8_t read_len = radio.getPayloadSize();

        radio.read(payload_buf, read_len);
        ESP_LOGI(TAG,
                 "RF24 raw packet received (len=%u)",
                 static_cast<unsigned>(read_len));
        receiver_handle_radio_payload(payload_buf, read_len);
        ++packets_processed;
    }

    if (radio.available()) {
        radio_irq_pending = true;
        ESP_LOGD(TAG,
                 "RF24 backlog detected, deferred remaining packets after %u reads",
                 static_cast<unsigned>(packets_processed));
    }
}

void receiver_test(void) {
    //constantly poll and print

    if (!radio_initialized) {
        ESP_LOGW(TAG, "RF24 not initialized, skipping test poll");
        vTaskDelay(pdMS_TO_TICKS(100));
        return;
    }

    if (!radio.available()) {
        ESP_LOGD(TAG, "RF24 no data available during test poll");
        vTaskDelay(pdMS_TO_TICKS(10));
        return;
    }

    uint8_t payload_buf[32] = {0};
    size_t payload_len = radio.getPayloadSize();

    if (payload_len > 32) {
        ESP_LOGW(TAG, "RF24 payload too large for test poll: %zu bytes", payload_len);
        return;
    }

    radio.read(payload_buf, payload_len);

    //drop if all 0
    if (payload_is_all_zero(payload_buf, payload_len)) {
        ESP_LOGD(TAG, "RF24 test poll received all-zero payload, ignoring");
        vTaskDelay(pdMS_TO_TICKS(10));
        return;
    }

    const size_t text_len = strnlen(reinterpret_cast<const char *>(payload_buf), payload_len);
    if (text_len < payload_len) {
        ESP_LOGI(TAG,
                 "RF24 test poll received text payload (%zu bytes): %.*s",
                 text_len,
                 static_cast<int>(text_len),
                 reinterpret_cast<const char *>(payload_buf));
    } else {
        ESP_LOGI(TAG,
                 "RF24 test poll received binary payload (%zu bytes)",
                 payload_len);
        ESP_LOG_BUFFER_HEX_LEVEL(TAG, payload_buf, payload_len, ESP_LOG_INFO);
    }

    vTaskDelay(pdMS_TO_TICKS(10));
}
