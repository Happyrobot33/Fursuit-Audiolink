#include <cstring>
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
#include "audio_processor.h"
#include "config.h"
#include "receiver.h"

// Global audio processor and synchronization
extern AudioProcessor audio_processor;
extern SemaphoreHandle_t audio_mutex;

static void receiver_espnow_callback(const esp_now_recv_info_t *recv_info, const uint8_t *data, int len) {
    /* Decode the received data as a Sub_Packet protobuf message */
    Sub_Packet sub_pkt;
    memset(&sub_pkt, 0, sizeof(sub_pkt));
    std::vector<uint8_t> pkt_data;
    
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
    ESP_LOGD(TAG, "Packet received - MAC: %02x:%02x:%02x:%02x:%02x:%02x, Index: %d, Count: %d, Data size: %zu",
             recv_info->src_addr[0], recv_info->src_addr[1], recv_info->src_addr[2],
             recv_info->src_addr[3], recv_info->src_addr[4], recv_info->src_addr[5],
             sub_pkt.packet_index, sub_pkt.packet_count, pkt_data.size());
    
    if (xSemaphoreTake(audio_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        /* Check if this is a new message (packet_index == 0) or continuation */
        if (sub_pkt.packet_index == 0) {
            /* New message: reset the buffer */
            audio_processor.reset();
            ESP_LOGD(TAG, "Starting new audio message (expecting %d packets)", sub_pkt.packet_count);
        }
        
        /* Append packet data to reconstructed buffer */
        if (audio_processor.reconstructed_data.size() + pkt_data.size() <= MAX_AUDIO_DATA_SIZE) {
            audio_processor.reconstructed_data.insert(
                audio_processor.reconstructed_data.end(),
                pkt_data.begin(),
                pkt_data.end());
            audio_processor.packet_count++;
            
            ESP_LOGD(TAG, "Received packet %d/%d (total buffered: %d bytes)",
                     sub_pkt.packet_index, sub_pkt.packet_count, 
                     static_cast<int>(audio_processor.reconstructed_data.size()));
            
            /* Try to reconstruct - if we have all expected packets */
            if (audio_processor.packet_count >= sub_pkt.packet_count) {
                try_reconstruct_audio_data(&audio_processor);
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
