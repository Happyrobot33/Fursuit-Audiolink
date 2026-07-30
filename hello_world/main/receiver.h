#pragma once

#include "audiolink_data.h"

/**
 * Initialize WiFi in STA mode (for ESP-NOW receiver)
 */
void receiver_wifi_init(void);

/**
 * Initialize and register ESP-NOW receiver callback
 */
void receiver_espnow_init(void);
