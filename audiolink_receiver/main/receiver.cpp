#include <algorithm>
#include <cstdint>
#include <cstring>
#include <vector>
#include <zlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/portmacro.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
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

extern "C" int64_t esp_timer_get_time(void);

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

static portMUX_TYPE receiver_stats_mux = portMUX_INITIALIZER_UNLOCKED;
static ReceiverPerfSnapshot receiver_perf = {};

static inline uint64_t now_us() {
    return static_cast<uint64_t>(esp_timer_get_time());
}

static void release_byte_vector_if_large(std::vector<uint8_t> &buffer) {
    if (buffer.capacity() > MAX_AUDIO_DATA_SIZE / 2) {
        std::vector<uint8_t>().swap(buffer);
    } else {
        buffer.clear();
    }
}

static void reset_reconstruction_state(bool release_capacity) {
    if (release_capacity) {
        std::vector<uint8_t>().swap(reconstructed_data);
        std::vector<std::vector<uint8_t>>().swap(packet_chunks);
    } else {
        reconstructed_data.clear();
        packet_chunks.clear();
    }

    packet_count = 0;
    expected_packet_count = 0;
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

static bool decode_streaming_zlib_payload(const std::vector<uint8_t> &compressed_data,
                                          AudiolinkData &decoded_audio,
                                          size_t *decompressed_size_out,
                                          uint64_t *zlib_time_us_out,
                                          uint64_t *nanopb_time_us_out) {
    ESP_LOGD(TAG, "Streaming inflate payload: compressed size=%zu bytes", compressed_data.size());
    if (compressed_data.empty()) {
        return false;
    }

    if (compressed_data.size() > MAX_AUDIO_DATA_SIZE) {
        ESP_LOGW(TAG, "Compressed payload exceeds maximum decode size: compressed=%zu max=%u",
                 compressed_data.size(),
                 MAX_AUDIO_DATA_SIZE);
        return false;
    }

    const size_t heap_limit = 16 * 1024;
    size_t free_heap = heap_caps_get_free_size(MALLOC_CAP_8BIT);
    size_t largest_block = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
    if (largest_block < heap_limit) {
        ESP_LOGW(TAG,
                 "Skipping zlib inflate: free_heap=%u largest_block=%u compressed_size=%zu",
                 static_cast<unsigned>(free_heap),
                 static_cast<unsigned>(largest_block),
                 compressed_data.size());
        return false;
    }

    static uint8_t decompressed_data[MAX_AUDIO_DATA_SIZE];
    const uint64_t zlib_start_us = now_us();
    bool decode_ok = false;
    size_t decompressed_size = 0;
    int zlib_result = Z_OK;

    for (int window_bits : { -MAX_WBITS, MAX_WBITS }) {
        z_stream zstream = {};
        zstream.next_in = const_cast<Bytef *>(compressed_data.data());
        zstream.avail_in = static_cast<uInt>(compressed_data.size());

        if (inflateInit2(&zstream, window_bits) != Z_OK) {
            continue;
        }

        decompressed_size = 0;
        zlib_result = Z_OK;

        do {
            zstream.next_out = decompressed_data + decompressed_size;
            zstream.avail_out = static_cast<uInt>(MAX_AUDIO_DATA_SIZE - decompressed_size);
            zlib_result = inflate(&zstream, Z_SYNC_FLUSH);

            if (zlib_result != Z_OK && zlib_result != Z_STREAM_END) {
                if (zlib_result == Z_MEM_ERROR) {
                    ESP_LOGW(TAG,
                             "ZLIB memory error: free_heap=%u largest_block=%u compressed_size=%zu window_bits=%d",
                             static_cast<unsigned>(free_heap),
                             static_cast<unsigned>(largest_block),
                             compressed_data.size(),
                             window_bits);
                } else {
                    ESP_LOGW(TAG, "ZLIB decompression failed with code %d (window_bits=%d)", zlib_result, window_bits);
                }
                inflateEnd(&zstream);
                break;
            }

            size_t produced = MAX_AUDIO_DATA_SIZE - decompressed_size - zstream.avail_out;
            decompressed_size += produced;

            if (zlib_result == Z_OK && zstream.avail_out == 0) {
                ESP_LOGW(TAG, "Decompressed payload exceeds maximum size limit (window_bits=%d)", window_bits);
                inflateEnd(&zstream);
                zlib_result = Z_BUF_ERROR;
                break;
            }
        } while (zlib_result != Z_STREAM_END);

        if (zlib_result == Z_STREAM_END) {
            inflateEnd(&zstream);
            break;
        }

        inflateEnd(&zstream);
    }

    if (zlib_time_us_out) {
        *zlib_time_us_out = now_us() - zlib_start_us;
    }

    if (zlib_result != Z_STREAM_END || decompressed_size == 0) {
        if (nanopb_time_us_out) {
            *nanopb_time_us_out = 0;
        }
        return false;
    }

    const uint64_t nanopb_start_us = now_us();
    pb_istream_t pb_stream = pb_istream_from_buffer(decompressed_data,
                                                    decompressed_size);

    decode_ok = NanoPb::decode<AudiolinkDataConverter>(pb_stream, decoded_audio);
    if (nanopb_time_us_out) {
        *nanopb_time_us_out = now_us() - nanopb_start_us;
    }

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
                                       AudiolinkData &decoded_out,
                                       uint64_t *zlib_time_us_out,
                                       uint64_t *nanopb_time_us_out) {
    if (compressed_data.empty()) {
        return false;
    }

    ESP_LOGD(TAG, "Attempting to reconstruct audio data from %zu bytes of compressed data", compressed_data.size());

    /* Pooled decode slots are reused; reset vectors before callback-driven append decode. */
    decoded_out = AudiolinkData{};

    size_t decompressed_size = 0;
    if (decode_streaming_zlib_payload(compressed_data,
                                      decoded_out,
                                      &decompressed_size,
                                      zlib_time_us_out,
                                      nanopb_time_us_out)) {
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
    const uint64_t callback_start_us = now_us();

    if (len > 0) {
        portENTER_CRITICAL(&receiver_stats_mux);
        receiver_perf.rx_packets++;
        receiver_perf.rx_total_bytes += static_cast<uint32_t>(len);
        portEXIT_CRITICAL(&receiver_stats_mux);
    }

    if (!pipeline_initialized) {
        const uint64_t callback_elapsed_us = now_us() - callback_start_us;
        portENTER_CRITICAL(&receiver_stats_mux);
        receiver_perf.rx_callback_time_us += callback_elapsed_us;
        portEXIT_CRITICAL(&receiver_stats_mux);
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
        const uint64_t callback_elapsed_us = now_us() - callback_start_us;
        portENTER_CRITICAL(&receiver_stats_mux);
        receiver_perf.rx_callback_time_us += callback_elapsed_us;
        portEXIT_CRITICAL(&receiver_stats_mux);
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
        reset_reconstruction_state(false);
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

                    portENTER_CRITICAL(&receiver_stats_mux);
                    receiver_perf.rx_completed_frames++;
                    portEXIT_CRITICAL(&receiver_stats_mux);
                } else {
                    ESP_LOGW(TAG, "No free compressed frame slot available");
                }

                reset_reconstruction_state(true);
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

    const uint64_t callback_elapsed_us = now_us() - callback_start_us;
    portENTER_CRITICAL(&receiver_stats_mux);
    receiver_perf.rx_callback_time_us += callback_elapsed_us;
    portEXIT_CRITICAL(&receiver_stats_mux);
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

    const uint64_t decode_start_us = now_us();
    uint64_t decode_zlib_us = 0;
    uint64_t decode_nanopb_us = 0;
    bool decode_ok = try_reconstruct_audio_data(*compressed_frame.data,
                                                compressed_frame.packet_count,
                                                *decoded_slot,
                                                &decode_zlib_us,
                                                &decode_nanopb_us);
    const uint64_t decode_elapsed_us = now_us() - decode_start_us;

    portENTER_CRITICAL(&receiver_stats_mux);
    receiver_perf.decode_attempts++;
    receiver_perf.decode_time_us += decode_elapsed_us;
    receiver_perf.decode_zlib_time_us += decode_zlib_us;
    receiver_perf.decode_nanopb_time_us += decode_nanopb_us;
    if (decode_ok) {
        receiver_perf.decode_successes++;
    } else {
        receiver_perf.decode_failures++;
    }
    portEXIT_CRITICAL(&receiver_stats_mux);

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

    release_byte_vector_if_large(*compressed_frame.data);
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

void receiver_take_perf_snapshot(ReceiverPerfSnapshot &out_snapshot) {
    portENTER_CRITICAL(&receiver_stats_mux);
    out_snapshot = receiver_perf;
    receiver_perf = {};
    portEXIT_CRITICAL(&receiver_stats_mux);
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
