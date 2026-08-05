#pragma once

#include "freertos/task.h"
#include "audiolink_data.h"

/**
 * Initialize WiFi in STA mode (for ESP-NOW receiver)
 */
void receiver_wifi_init(void);

/**
 * Initialize and register ESP-NOW receiver callback
 */
void receiver_espnow_init(void);

/**
 * Process completed frames outside the ESP-NOW callback context
 */
void receiver_process_pending(void);

/**
 * Register the worker task handle that should be notified when a frame is ready.
 */
void receiver_set_process_task_handle(TaskHandle_t task_handle);

/**
 * Retrieve one decoded audio frame if available.
 */
bool receiver_take_decoded_frame(AudiolinkData &out_audio, TickType_t wait_ticks);
