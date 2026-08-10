#include <algorithm>
#include <cstring>
#include <vector>
#include <zlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_now.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "nvs_flash.h"
#include "audiolink_data.pb.h"
#include "pb_decode.h"
#include "nanopb_cpp.h"
#include "config.h"
#include "receiver.h"
#include "audio_converters.hpp"

using namespace NanoPb::Converter;

// Local working buffers for packet reconstruction
static std::vector<uint8_t> reconstructed_data;
static std::vector<std::vector<uint8_t>> packet_chunks;
static int packet_count = 0;
static int expected_packet_count = 0;
static TaskHandle_t process_task_handle = nullptr;

constexpr size_t FRAME_QUEUE_DEPTH = 2;

struct CompressedFrame {
    std::vector<uint8_t> *data;
    int packet_count;
};

static QueueHandle_t free_compressed_queue = nullptr;
static QueueHandle_t ready_compressed_queue = nullptr;
static QueueHandle_t free_decoded_queue = nullptr;
static QueueHandle_t ready_decoded_queue = nullptr;
static bool pipeline_initialized = false;

static std::vector<uint8_t> compressed_frame_pool[FRAME_QUEUE_DEPTH];
static AudiolinkData decoded_audio_pool[FRAME_QUEUE_DEPTH];

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

static void receiver_espnow_callback(const esp_now_recv_info_t *recv_info, const uint8_t *data, int len) {
    if (!pipeline_initialized) {
        return;
    }

    /* Decode the received data as a Sub_Packet protobuf message */
    PROTO_Sub_Packet sub_pkt;
    memset(&sub_pkt, 0, sizeof(sub_pkt));
    std::vector<uint8_t> pkt_data;
    
    /* Set up callback for data field */
    sub_pkt.data.funcs.decode = decode_subpacket_data_callback;
    sub_pkt.data.arg = &pkt_data;
    
    pb_istream_t stream = pb_istream_from_buffer(data, len);
    bool decode_status = pb_decode(&stream, PROTO_Sub_Packet_fields, &sub_pkt);
    
    if (!decode_status) {
        ESP_LOGW(TAG, "Failed to decode Sub_Packet: %s", PB_GET_ERROR(&stream));
        return;
    }
    
    /* Use debug logging to avoid blocking in callback */
    ESP_LOGD(TAG, "Packet received - MAC: %02x:%02x:%02x:%02x:%02x:%02x, Index: %d, Count: %d, Data size: %zu",
             recv_info->src_addr[0], recv_info->src_addr[1], recv_info->src_addr[2],
             recv_info->src_addr[3], recv_info->src_addr[4], recv_info->src_addr[5],
             sub_pkt.packet_index, sub_pkt.packet_count, pkt_data.size());

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
            packet_chunks[sub_pkt.packet_index] = pkt_data;
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



void receiver_wifi_init(void) {
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());
}

void receiver_espnow_init(void) {
    ESP_ERROR_CHECK_WITHOUT_ABORT(receiver_init_pipeline_queues() ? ESP_OK : ESP_FAIL);
    ESP_ERROR_CHECK(esp_now_init());
    ESP_ERROR_CHECK(esp_now_register_recv_cb(receiver_espnow_callback));
    ESP_LOGI(TAG, "ESP-NOW receiver ready (broadcast) - ESP-NOW v2 mode with 1000+ byte support");
}
