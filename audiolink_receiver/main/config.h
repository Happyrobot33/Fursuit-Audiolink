#pragma once

#include <cstdint>
#include "driver/gpio.h"

// GPIO assignment
#define LED_STRIP_BLINK_GPIO  GPIO_NUM_13

// HUB75 64x32 matrix pin assignment
#define MATRIX_R1  GPIO_NUM_25
#define MATRIX_G1  GPIO_NUM_26
#define MATRIX_B1  GPIO_NUM_27
#define MATRIX_R2  GPIO_NUM_14
#define MATRIX_G2  GPIO_NUM_12
#define MATRIX_B2  GPIO_NUM_13
#define MATRIX_A   GPIO_NUM_23
#define MATRIX_B   GPIO_NUM_19
#define MATRIX_C   GPIO_NUM_5
#define MATRIX_D   GPIO_NUM_17
#define MATRIX_LAT GPIO_NUM_4
#define MATRIX_OE  GPIO_NUM_15
#define MATRIX_CLK GPIO_NUM_22

// HUB75 matrix dimensions
static constexpr uint16_t MATRIX_WIDTH = 64;
static constexpr uint16_t MATRIX_HEIGHT = 32;

// Selects which output device the receiver renders audio data to
enum class OutputDevice : uint8_t {
    LedStrip,
    Matrix,
};
// static constexpr OutputDevice ACTIVE_OUTPUT_DEVICE = OutputDevice::LedStrip;
static constexpr OutputDevice ACTIVE_OUTPUT_DEVICE = OutputDevice::Matrix;

// Number of LEDs in the strip
static constexpr uint16_t LED_STRIP_LED_NUMBERS = 60 * 2;

// Selects which IShader implementations are used; see Rendering/shader_config.h.
static constexpr uint32_t FALLBACK_TIMEOUT_MS = 10000;

// 10MHz resolution, 1 tick = 0.1us
#define LED_STRIP_RMT_RES_HZ  (10 * 1000 * 1000)

// Buffer for receiving Sub_Packets and reconstructing Audiolink_Data
#define MAX_SUB_PACKETS 20
#define MAX_AUDIO_DATA_SIZE 46000

static const char *TAG [[maybe_unused]] = "audiolink";
