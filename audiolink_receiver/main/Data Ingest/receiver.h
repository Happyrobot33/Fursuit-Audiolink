#pragma once

#include <cstdint>
#include "freertos/task.h"
#include "audiolink_data.h"

struct ReceiverPerfSnapshot {
	uint32_t rx_packets = 0;
	uint32_t rx_completed_frames = 0;
	uint32_t rx_total_bytes = 0;
	uint64_t rx_callback_time_us = 0;
	uint32_t decode_attempts = 0;
	uint32_t decode_successes = 0;
	uint32_t decode_failures = 0;
	uint64_t decode_time_us = 0;
	uint64_t decode_zlib_time_us = 0;
	uint64_t decode_nanopb_time_us = 0;
};

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

/**
 * Capture and reset receiver timing counters used for frame-time attribution logs.
 */
void receiver_take_perf_snapshot(ReceiverPerfSnapshot &out_snapshot);
