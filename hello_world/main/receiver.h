#pragma once

#include <vector>

/**
 * C++ wrapper struct for History with vectors instead of callbacks
 */
struct CppHistory {
    std::vector<float> bass;
    std::vector<float> lowmid;
    std::vector<float> highmid;
    std::vector<float> treble;
};

/**
 * C++ wrapper struct for Audiolink_Data with vectors
 */
struct CppAudiolinkData {
    CppHistory history;
};

/**
 * Initialize WiFi in STA mode (for ESP-NOW receiver)
 */
void receiver_wifi_init(void);

/**
 * Initialize and register ESP-NOW receiver callback
 */
void receiver_espnow_init(void);
