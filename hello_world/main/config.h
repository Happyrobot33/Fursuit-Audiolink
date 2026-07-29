#pragma once

// GPIO assignment
#define LED_STRIP_BLINK_GPIO  GPIO_NUM_13

// Number of LEDs in the strip
#define LED_STRIP_LED_NUMBERS 60

// 10MHz resolution, 1 tick = 0.1us
#define LED_STRIP_RMT_RES_HZ  (10 * 1000 * 1000)

// Buffer for receiving Sub_Packets and reconstructing Audiolink_Data
#define MAX_SUB_PACKETS 16
#define MAX_AUDIO_DATA_SIZE 10000

static const char *TAG = "audiolink";
