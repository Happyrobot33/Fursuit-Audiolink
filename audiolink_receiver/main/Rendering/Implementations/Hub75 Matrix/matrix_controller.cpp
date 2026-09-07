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
