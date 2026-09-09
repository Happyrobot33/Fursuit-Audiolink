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
#define MATRIX_E   GPIO_NUM_21 //set to -1 to disable
#define MATRIX_LAT GPIO_NUM_4
#define MATRIX_OE  GPIO_NUM_15
#define MATRIX_CLK GPIO_NUM_22

// Invert I2S clock phase; flip if the panel shows garbled/shifted pixels.
// #define MATRIX_CLOCK_PHASE_INVERT true
#define MATRIX_CLOCK_PHASE_INVERT false

// HUB75 matrix dimensions
//the width and height should match what an INDIVIDUAL panel looks like
#define MATRIX_WIDTH  64
#define MATRIX_HEIGHT 64
#define MATRIX_COUNT 2
// EDIT THESE FOR THE LAYOUT OF YOUR PANELS
#define SCREEN_WIDTH  MATRIX_WIDTH * MATRIX_COUNT
#define SCREEN_HEIGHT MATRIX_HEIGHT

// Selects which output device the receiver renders audio data to
enum class OutputDevice : uint8_t {
    LedStrip,
    Matrix,
    MatrixStrip,
};
// static constexpr OutputDevice ACTIVE_OUTPUT_DEVICE = OutputDevice::LedStrip;
static constexpr OutputDevice ACTIVE_OUTPUT_DEVICE = OutputDevice::Matrix;
// static constexpr OutputDevice ACTIVE_OUTPUT_DEVICE = OutputDevice::MatrixStrip;

// Number of LEDs in the strip. For a strip-driven matrix, the strip must be large enough
// to cover every pixel in the logical matrix.
// static constexpr uint16_t LED_STRIP_LED_NUMBERS = 60 * 2;
// static constexpr uint16_t LED_STRIP_LED_NUMBERS = 8 * 2; //sticks
static constexpr uint16_t LED_STRIP_LED_NUMBERS = MATRIX_WIDTH * MATRIX_HEIGHT;
// Brightness tuning for both output devices. 0..255, where 255 is full brightness.
//LED strip still applies to strip matrices
static constexpr uint8_t LED_STRIP_BRIGHTNESS = 20;
static constexpr uint8_t MATRIX_BRIGHTNESS = 128;
// Selects which IShader implementations are used; see Rendering/shader_config.h.
static constexpr uint32_t FALLBACK_TIMEOUT_MS = 1 * 1000;

// 10MHz resolution, 1 tick = 0.1us
#define LED_STRIP_RMT_RES_HZ  (10 * 1000 * 1000)

// Buffer for receiving Sub_Packets and reconstructing Audiolink_Data
#define MAX_SUB_PACKETS 6
#define MAX_AUDIO_DATA_SIZE 15000

static const char *TAG [[maybe_unused]] = "audiolink";
