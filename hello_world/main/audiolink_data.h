#pragma once

#include <vector>

struct Color {
    float r;
    float g;
    float b;
};

struct ThemeColors {
    Color ThemeColor0;
    Color ThemeColor1;
    Color ThemeColor2;
    Color ThemeColor3;
};

/**
 * C++ wrapper struct for History with vectors instead of callbacks
 */
struct History {
    std::vector<float> bass;
    std::vector<float> lowmid;
    std::vector<float> highmid;
    std::vector<float> treble;
};

/**
 * C++ wrapper struct for Audiolink_Data with vectors
 */
struct AudiolinkData {
    History history;
    ThemeColors theme_colors;
};
