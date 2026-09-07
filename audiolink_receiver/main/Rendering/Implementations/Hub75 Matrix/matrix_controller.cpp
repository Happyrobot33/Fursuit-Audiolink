#include "matrix_controller.h"

#include <algorithm>

#include "config.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "ESP32-HUB75-MatrixPanel-I2S-DMA.h"

esp_err_t MatrixController::init() {
    if (driver_ != nullptr) {
        return ESP_OK;
    }

    HUB75_I2S_CFG::i2s_pins pins = {
        MATRIX_R1, MATRIX_G1, MATRIX_B1,
        MATRIX_R2, MATRIX_G2, MATRIX_B2,
        MATRIX_A, MATRIX_B, MATRIX_C, MATRIX_D, -1,
        MATRIX_LAT, MATRIX_OE, MATRIX_CLK,
    };
    HUB75_I2S_CFG config(MATRIX_WIDTH,
                         MATRIX_HEIGHT,
                         1,
                         pins,
                         HUB75_I2S_CFG::SHIFTREG,
                         HUB75_I2S_CFG::TYPE138,
                         true,
                         HUB75_I2S_CFG::HZ_10M,
                         5, //latching delay. TODO: Experiment with this more to fix flickering possibly
                         true,
                         60,
                         8);

    //explicelty set latch speed
    // config.latch_blanking = 8; // Set latch speed to 8 (example value)

    driver_ = new MatrixPanel_I2S_DMA(config);
    if (!driver_->begin()) {
        delete driver_;
        driver_ = nullptr;
        return ESP_FAIL;
    }

    return ESP_OK;
}

void MatrixController::deinit() {
    if (driver_ == nullptr) {
        return;
    }

    delete driver_;
    driver_ = nullptr;
}

void MatrixController::render_dft(const std::vector<float>& magnitudes) {
    if (driver_ == nullptr || magnitudes.empty()) {
        return;
    }

    constexpr uint16_t matrix_width = MATRIX_WIDTH;
    constexpr uint16_t matrix_height = MATRIX_HEIGHT;
    float peak = 1.0f;

    driver_->clearScreen();
    for (uint16_t x = 0; x < matrix_width; ++x) {
        size_t index = (static_cast<size_t>(x) * magnitudes.size()) / matrix_width;
        index = std::min(index, magnitudes.size() - 1);
        float normalized = std::clamp(magnitudes[index] / peak, 0.0f, 1.0f);
        float normalized_x = static_cast<float>(x) / static_cast<float>(matrix_width);
        uint16_t bar_height = static_cast<uint16_t>(normalized * matrix_height);

        // Map normalized value (0-1) to color based on quarters
        uint8_t r, g, b;
        if (normalized_x < 0.25f) {
            // Quarter 1: Red
            r = 255;
            g = 0;
            b = 0;
        } else if (normalized_x < 0.5f) {
            // Quarter 2: Yellow
            r = 255;
            g = 255;
            b = 0;
        } else if (normalized_x < 0.75f) {
            // Quarter 3: Green
            r = 0;
            g = 255;
            b = 0;
        } else {
            // Quarter 4: Blue
            r = 0;
            g = 0;
            b = 255;
        }

        for (uint16_t row = 0; row < bar_height; ++row) {
            uint16_t y = matrix_height - 1 - row;
            driver_->drawPixelRGB888(x, y, r, g, b);
        }
    }
    driver_->flipDMABuffer();
}

void MatrixController::render_sample_pattern() {
    if (driver_ == nullptr) {
        return;
    }

    constexpr uint16_t matrix_width = MATRIX_WIDTH;
    constexpr uint16_t matrix_height = MATRIX_HEIGHT;
    constexpr uint16_t bar_width = matrix_width / 8;
    constexpr uint8_t colors[8][3] = {
        {255, 0, 0},
        {255, 128, 0},
        {255, 255, 0},
        {0, 255, 0},
        {0, 255, 255},
        {0, 0, 255},
        {128, 0, 255},
        {255, 0, 128},
    };
    const uint16_t color_shift =
        static_cast<uint16_t>((xTaskGetTickCount() * portTICK_PERIOD_MS / 200) % 8);

    driver_->clearScreen();
    for (uint16_t x = 0; x < matrix_width; ++x) {
        uint16_t color_index = (x / bar_width + color_shift) % 8;
        for (uint16_t y = 0; y < matrix_height; ++y) {
            uint8_t brightness = static_cast<uint8_t>(255 - (y * 160 / matrix_height));
            driver_->drawPixelRGB888(x,
                                     y,
                                     colors[color_index][0] * brightness / 255,
                                     colors[color_index][1] * brightness / 255,
                                     colors[color_index][2] * brightness / 255);
        }
    }
    driver_->flipDMABuffer();
}

void MatrixController::fill(const Color& color) {
    if (driver_ == nullptr) {
        return;
    }

    constexpr uint16_t matrix_width = MATRIX_WIDTH;
    constexpr uint16_t matrix_height = MATRIX_HEIGHT;

    driver_->clearScreen();
    for (uint16_t x = 0; x < matrix_width; ++x) {
        for (uint16_t y = 0; y < matrix_height; ++y) {
            driver_->drawPixelRGB888(x, y,
                                     static_cast<uint8_t>(color.R * 255),
                                     static_cast<uint8_t>(color.G * 255),
                                     static_cast<uint8_t>(color.B * 255));
        }
    }
    driver_->flipDMABuffer();
}

void MatrixController::set_pixel(uint16_t x, uint16_t y, const Color& color) {
    if (driver_ == nullptr) {
        return;
    }

    driver_->drawPixelRGB888(x, y,
                             static_cast<uint8_t>(color.R * 255),
                             static_cast<uint8_t>(color.G * 255),
                             static_cast<uint8_t>(color.B * 255));
}

void MatrixController::clear() {
    if (driver_ == nullptr) {
        return;
    }

    driver_->clearScreen();
}

void MatrixController::present() {
    if (driver_ == nullptr) {
        return;
    }

    driver_->flipDMABuffer();
}
