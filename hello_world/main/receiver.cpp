#include <cstring>
#include <vector>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
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

// Global audio data and synchronization
extern CppAudiolinkData audio_data;
extern bool audio_complete;
extern SemaphoreHandle_t audio_mutex;

// Local working buffers for packet reconstruction
static std::vector<uint8_t> reconstructed_data;
static int packet_count = 0;

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

static void try_reconstruct_audio_data() {
    if (reconstructed_data.empty()) {
        return;
    }
    
    /* Decode into temporary local struct */
    CppAudiolinkData temp_audio;
    pb_istream_t stream = pb_istream_from_buffer(reconstructed_data.data(), 
                                               reconstructed_data.size());
    
    if (NanoPb::decode<AudiolinkDataConverter>(stream, temp_audio)) {
        /* Atomic update: copy decoded data to global */
        audio_data = temp_audio;
        audio_complete = true;
        ESP_LOGI(TAG, "Audio data reconstructed: received %d packets, bass=%zu, lowmid=%zu, highmid=%zu, treble=%zu",
                 packet_count,
                 audio_data.history.bass.size(),
                 audio_data.history.lowmid.size(),
                 audio_data.history.highmid.size(),
                 audio_data.history.treble.size());
    } else {
        ESP_LOGW(TAG, "Protobuf decode failed: %s", PB_GET_ERROR(&stream));
    }
}

static void receiver_espnow_callback(const esp_now_recv_info_t *recv_info, const uint8_t *data, int len) {
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
    
    if (xSemaphoreTake(audio_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        /* Check if this is a new message (packet_index == 0) or continuation */
        if (sub_pkt.packet_index == 0) {
            /* New message: reset the buffer */
            reconstructed_data.clear();
            packet_count = 0;
            audio_complete = false;
            ESP_LOGD(TAG, "Starting new audio message (expecting %d packets)", sub_pkt.packet_count);
        }
        
        /* Append packet data to reconstructed buffer */
        if (reconstructed_data.size() + pkt_data.size() <= MAX_AUDIO_DATA_SIZE) {
            reconstructed_data.insert(
                reconstructed_data.end(),
                pkt_data.begin(),
                pkt_data.end());
            packet_count++;
            
            ESP_LOGD(TAG, "Received packet %d/%d (total buffered: %d bytes)",
                     sub_pkt.packet_index, sub_pkt.packet_count, 
                     static_cast<int>(reconstructed_data.size()));
            
            /* Try to reconstruct - if we have all expected packets */
            if (packet_count >= sub_pkt.packet_count) {
                try_reconstruct_audio_data();
            }
        } else {
            ESP_LOGW(TAG, "Audio data too large, discarding");
        }
        
        xSemaphoreGive(audio_mutex);
    }
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
    ESP_ERROR_CHECK(esp_now_init());
    ESP_ERROR_CHECK(esp_now_register_recv_cb(receiver_espnow_callback));
    ESP_LOGI(TAG, "ESP-NOW receiver ready (broadcast) - ESP-NOW v2 mode with 1000+ byte support");
}
