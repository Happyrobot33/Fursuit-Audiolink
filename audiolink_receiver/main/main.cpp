/*
 * SPDX-FileCopyrightText: 2010-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#include <cstdio>
#include <cmath>
#include <memory>
#include <vector>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "color_utils.h"
#include "render_target.h"
#include "render_target_factory.h"
#include "renderer.h"
#include "shader_factory.h"
#include "receiver.h"
#include "config.h"
#include <ctime>

// Rendering is fully abstracted behind IRenderTarget/IShader; main has no knowledge of the
// concrete matrix/LED-strip controllers or shader implementation.
static std::unique_ptr<IRenderTarget> g_render_target;
static std::unique_ptr<IShader> g_shader;
static std::unique_ptr<IShader> g_fallback_shader;

// Tracks whether real audio data is fresh enough to render with g_shader.
static AudiolinkData g_last_audio_data;
static uint32_t g_last_data_ms = 0;
static bool g_has_received_data = false;

// Framerate tracking
static uint32_t frame_count = 0;
static uint32_t last_log_time_ms = 0;

// CONFIG_FREERTOS_HZ=100 makes pdMS_TO_TICKS() truncate to 0 for ms values below 10,
// and vTaskDelay(0) never yields to a lower-priority task (e.g. IDLE), starving the WDT.
static inline TickType_t min_delay_ticks(uint32_t ms) {
    const TickType_t ticks = pdMS_TO_TICKS(ms);
    return ticks > 0 ? ticks : 1;
}

static void receiver_process_task(void *arg) {
    while (1) {
        /* Block until callback signals a completed frame, with periodic timeout as safeguard. */
        (void)ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(50));
        receiver_process_pending();
        vTaskDelay(min_delay_ticks(1));
    }
}

static void led_update_task(void *arg) {
    while (1) {
        AudiolinkData local_audio_data;

        bool should_update = receiver_take_decoded_frame(local_audio_data, pdMS_TO_TICKS(5));

        const bool output_ready = static_cast<bool>(g_render_target);
        const uint32_t current_time_ms = xTaskGetTickCount() * portTICK_PERIOD_MS;

        if (should_update) {
            g_last_audio_data = local_audio_data;
            g_last_data_ms = current_time_ms;
            g_has_received_data = true;
        }

        const bool data_is_stale = !g_has_received_data ||
            (current_time_ms - g_last_data_ms) > FALLBACK_TIMEOUT_MS;

        if (output_ready && (should_update || data_is_stale)) {
            frame_count++;
            uint32_t elapsed_time_ms = current_time_ms - last_log_time_ms;
            if (elapsed_time_ms >= 1000) {
                float framerate = (frame_count * 1000.0f) / elapsed_time_ms;
                if (data_is_stale) {
                    ESP_LOGI(TAG, "Fallback framerate: %.1f FPS", framerate);
                } else {
                    ReceiverPerfSnapshot perf = {};
                    receiver_take_perf_snapshot(perf);

                    const float window_ms = static_cast<float>(elapsed_time_ms);
                    const float rx_ms = static_cast<float>(perf.rx_callback_time_us) / 1000.0f;
                    const float decode_ms = static_cast<float>(perf.decode_time_us) / 1000.0f;
                    const float decode_zlib_ms = static_cast<float>(perf.decode_zlib_time_us) / 1000.0f;
                    const float decode_nanopb_ms = static_cast<float>(perf.decode_nanopb_time_us) / 1000.0f;
                    const float rx_share_pct = window_ms > 0.0f ? (rx_ms * 100.0f / window_ms) : 0.0f;
                    const float decode_share_pct = window_ms > 0.0f ? (decode_ms * 100.0f / window_ms) : 0.0f;
                    const float decode_zlib_share_pct = window_ms > 0.0f ? (decode_zlib_ms * 100.0f / window_ms) : 0.0f;
                    const float decode_nanopb_share_pct = window_ms > 0.0f ? (decode_nanopb_ms * 100.0f / window_ms) : 0.0f;

                    ESP_LOGI(TAG,
                             "Framerate: %.1f FPS | recv: %.2fms (%.1f%%, packets=%u bytes=%u frames=%u) | decode: %.2fms (%.1f%%, zlib %.2fms/%.1f%%, nanopb %.2fms/%.1f%%, ok=%u fail=%u)",
                             framerate,
                             rx_ms,
                             rx_share_pct,
                             perf.rx_packets,
                             perf.rx_total_bytes,
                             perf.rx_completed_frames,
                             decode_ms,
                             decode_share_pct,
                             decode_zlib_ms,
                             decode_zlib_share_pct,
                             decode_nanopb_ms,
                             decode_nanopb_share_pct,
                             perf.decode_successes,
                             perf.decode_failures);
                }
                frame_count = 0;
                last_log_time_ms = current_time_ms;
            }
        }

        if (output_ready) {
            IShader &active_shader = data_is_stale ? *g_fallback_shader : *g_shader;
            render_shader_frame(*g_render_target, active_shader, g_last_audio_data);
        }

        vTaskDelay(min_delay_ticks(should_update ? 5 : 15));
    }
}

extern "C" void app_main(void) {
    g_render_target = create_render_target();
    g_shader = create_shader();
    g_fallback_shader = create_fallback_shader();

    /* Start LED rendering on CPU 1 */
    BaseType_t task_created = xTaskCreatePinnedToCore(
        led_update_task,
        "led_update_task",
        4096,
        nullptr,
        2,
        nullptr,
        1);
    if (task_created != pdPASS) {
        ESP_LOGE(TAG, "Failed to create LED update task");
    }

    /* Run decompress/decode work on CPU0 to keep CPU1 IDLE alive for task watchdog */
    TaskHandle_t receiver_task_handle = nullptr;
    BaseType_t receiver_task_created = xTaskCreatePinnedToCore(
        receiver_process_task,
        "receiver_process_task",
        6144,
        nullptr,
        2,
        &receiver_task_handle,
        0);
    if (receiver_task_created != pdPASS) {
        ESP_LOGE(TAG, "Failed to create receiver process task");
    } else {
        receiver_set_process_task_handle(receiver_task_handle);
    }

    /* Initialize WiFi and ESP-NOW */
    receiver_wifi_init();
    receiver_espnow_init();

    ESP_LOGI(TAG, "Application started - waiting for audio data...");

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
