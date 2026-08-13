/*
 * SPDX-FileCopyrightText: 2010-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#include <cstdio>
#include <vector>
//TODO: Figure out how to add this to idf_component.yml without cmake complaining due to esp-idf
#include "include/magic_enum/magic_enum.hpp"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "led_controller.h"
#include "receiver.h"
#include "config.h"

// Global data structures
LEDController led_controller;

// Framerate tracking
static uint32_t frame_count = 0;
static uint32_t last_log_time_ms = 0;
static led_strip_handle_t g_led_strip = nullptr;
static LEDController *g_led_controller = nullptr;

static constexpr gpio_num_t BOOT_BUTTON_GPIO = GPIO_NUM_0;
static constexpr TickType_t BOOT_BUTTON_DEBOUNCE_TICKS = pdMS_TO_TICKS(200);

enum class LedMappingMode : uint8_t {
    Bass = 0,
    Lowmid,
    Highmid,
    Treble,
    ColorChordStrip,
    ColorChordLights,
    Dft,
    BassFiltered,
    LowmidFiltered,
    HighmidFiltered,
    TrebleFiltered,
    Count
};

static LedMappingMode g_mapping_mode = LedMappingMode::Bass;
static int g_last_button_level = 1;
static TickType_t g_last_button_change_tick = 0;
static bool g_button_pressed_latched = false;

static void cycle_mapping_mode() {
    uint8_t next_mode = static_cast<uint8_t>(g_mapping_mode) + 1;
    if (next_mode >= static_cast<uint8_t>(LedMappingMode::Count)) {
        next_mode = 0;
    }
    g_mapping_mode = static_cast<LedMappingMode>(next_mode);
    const auto mode_name = magic_enum::enum_name(g_mapping_mode);
    ESP_LOGI(TAG,
             "LED mapping switched to: %.*s",
             static_cast<int>(mode_name.size()),
             mode_name.data());
}

static float clamp01(float value) {
    if (value < 0.0f) {
        return 0.0f;
    }
    if (value > 1.0f) {
        return 1.0f;
    }
    return value;
}

static void render_frequency_mapping(const std::vector<float> &frequency_values, const Color &color) {
    if (!g_led_controller || !g_led_strip || frequency_values.empty()) {
        return;
    }

    g_led_controller->map_to_leds(g_led_strip, frequency_values, 0, LED_STRIP_LED_NUMBERS, color);
}

static void render_color_mapping(const std::vector<Color> &colors) {
    if (!g_led_controller || !g_led_strip || colors.empty()) {
        return;
    }

    const int led_count = LED_STRIP_LED_NUMBERS;
    const size_t color_count = colors.size();
    for (int led_index = 0; led_index < led_count; ++led_index) {
        size_t idx = (static_cast<size_t>(led_index) * color_count) /
                     static_cast<size_t>(led_count);
        if (idx >= color_count) {
            idx = color_count - 1;
        }
        const Color &src = colors[idx];
        g_led_controller->set_pixel(g_led_strip,
                                    led_index,
                                    Color{clamp01(src.R), clamp01(src.G), clamp01(src.B)});
    }
}

static void render_selected_mapping(const AudiolinkData &audio_data) {
    switch (g_mapping_mode) {
        case LedMappingMode::Bass:
            render_frequency_mapping(audio_data.history.bass, Color{1.0f, 0.0f, 0.0f});
            break;
        case LedMappingMode::Lowmid:
            render_frequency_mapping(audio_data.history.lowmid, Color{1.0f, 0.5f, 0.0f});
            break;
        case LedMappingMode::Highmid:
            render_frequency_mapping(audio_data.history.highmid, Color{0.0f, 1.0f, 0.0f});
            break;
        case LedMappingMode::Treble:
            render_frequency_mapping(audio_data.history.treble, Color{0.0f, 0.5f, 1.0f});
            break;
        case LedMappingMode::ColorChordStrip:
            render_color_mapping(audio_data.colorchord.strip);
            break;
        case LedMappingMode::ColorChordLights:
            render_color_mapping(audio_data.colorchord.lights);
            break;
        case LedMappingMode::Dft:
            render_frequency_mapping(audio_data.dft.mag, Color{0.7f, 0.0f, 1.0f});
            break;
        case LedMappingMode::BassFiltered:
            render_frequency_mapping(audio_data.filtered_audiolink.bass, Color{1.0f, 0.0f, 0.3f});
            break;
        case LedMappingMode::LowmidFiltered:
            render_frequency_mapping(audio_data.filtered_audiolink.lowmid, Color{1.0f, 0.7f, 0.0f});
            break;
        case LedMappingMode::HighmidFiltered:
            render_frequency_mapping(audio_data.filtered_audiolink.highmid, Color{0.0f, 1.0f, 0.3f});
            break;
        case LedMappingMode::TrebleFiltered:
            render_frequency_mapping(audio_data.filtered_audiolink.treble, Color{0.2f, 0.7f, 1.0f});
            break;
        default:
            break;
    }
}

static void boot_button_init() {
    gpio_config_t io_conf = {};
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pin_bit_mask = (1ULL << BOOT_BUTTON_GPIO);
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    ESP_ERROR_CHECK(gpio_config(&io_conf));

    g_last_button_level = gpio_get_level(BOOT_BUTTON_GPIO);
    g_last_button_change_tick = xTaskGetTickCount();
    g_button_pressed_latched = false;
    ESP_LOGI(TAG, "BOOT button ready on GPIO %d", static_cast<int>(BOOT_BUTTON_GPIO));
}

static void poll_boot_button() {
    int current_level = gpio_get_level(BOOT_BUTTON_GPIO);
    TickType_t now = xTaskGetTickCount();

    if (current_level != g_last_button_level) {
        g_last_button_level = current_level;
        g_last_button_change_tick = now;
    }

    if ((now - g_last_button_change_tick) < BOOT_BUTTON_DEBOUNCE_TICKS) {
        return;
    }

    const bool button_pressed = (current_level == 0);
    if (button_pressed && !g_button_pressed_latched) {
        g_button_pressed_latched = true;
        cycle_mapping_mode();
    } else if (!button_pressed && g_button_pressed_latched) {
        g_button_pressed_latched = false;
    }
}

static void receiver_process_task(void *arg) {
    while (1) {
        /* Block until callback signals a completed frame, with periodic timeout as safeguard. */
        (void)ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(50));
        receiver_process_pending();
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}

static void led_update_task(void *arg) {
    while (1) {
        AudiolinkData local_audio_data;

        poll_boot_button();

        bool should_update = receiver_take_decoded_frame(local_audio_data, pdMS_TO_TICKS(5));

        if (should_update && g_led_controller && g_led_strip) {
            uint32_t current_time_ms = xTaskGetTickCount() * portTICK_PERIOD_MS;
            frame_count++;
            uint32_t elapsed_time_ms = current_time_ms - last_log_time_ms;
            if (elapsed_time_ms >= 1000) {
                float framerate = (frame_count * 1000.0f) / elapsed_time_ms;
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
                frame_count = 0;
                last_log_time_ms = current_time_ms;
            }

            g_led_controller->clear(g_led_strip);
            // Previous bass-based rendering path (kept for quick fallback/testing):
            // g_led_controller->map_to_leds(g_led_strip,
            //                               local_audio_data.history.bass,
            //                               0,
            //                               LED_STRIP_LED_NUMBERS,
            //                               Color{1.0f, 0.0f, 0.0f}); // Red for bass
            render_selected_mapping(local_audio_data);
            ESP_ERROR_CHECK(led_strip_refresh(g_led_strip));
        }

        vTaskDelay(pdMS_TO_TICKS(should_update ? 5 : 15));
    }
}

extern "C" void app_main(void) {
    /* Initialize GPIO and LED strip */
    boot_button_init();
    gpio_set_drive_capability(LED_STRIP_BLINK_GPIO, GPIO_DRIVE_CAP_3);
    led_strip_handle_t led_strip = led_controller.init();
    g_led_strip = led_strip;
    g_led_controller = &led_controller;

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
