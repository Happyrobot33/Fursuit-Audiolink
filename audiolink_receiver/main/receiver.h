#pragma once

#include "freertos/task.h"
#include "audiolink_data.h"

/**
 * Initialize RF24 radio receiver.
 */
void receiver_rf24_init(void);

/**
 * Poll RF24 for newly received packets and queue complete frames.
 */
void receiver_rf24_poll(void);

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

void receiver_test(void);
