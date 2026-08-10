#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sender.h"
#include "receiver.h"
#include "shared.h"

static const char *TAG = "app_main";

/* Global audio queue */
QueueHandle_t audio_queue = NULL;

extern "C" void app_main(void)
{
    ESP_LOGI(TAG, "Application starting");
    ESP_ERROR_CHECK(nvs_flash_init());

    setup_radio();

    /* Create queue for passing decoded audio frames to sender task */
    audio_queue = xQueueCreate(10, sizeof(QueuedAudioFrame));
    if (audio_queue == NULL) {
        ESP_LOGE(TAG, "Failed to create audio queue");
        return;
    }

    /* Create ESP-NOW sender task */
    xTaskCreate(sender_task, "espnow_sender", 16384, NULL, 6, NULL);
    /* Create UART reader task */
    xTaskCreate(serial_rx_task, "serial_rx", 16384, NULL, 5, NULL);

    ESP_LOGI(TAG, "Tasks created, application running");
}
