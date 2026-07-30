#pragma once

#include <vector>
#include <cstdint>

//TODO: Eventually use a custom type here instead that handles the float and uint32 conversion

/**
 * Helper function to convert float to float [0.0, 1.0]
 * uint32 value represents a normalized float where:
 * 0 -> 0.0f, UINT32_MAX -> 1.0f
 */
inline float toNorm(float value) {
    return static_cast<float>(value) / 0xFFFFFFFFu;
}

/**
 * Helper function to convert float [0.0, 1.0] to float
 */
inline float fromNorm(float f) {
    return static_cast<float>(f * 0xFFFFFFFFu);
}

struct Color {
    float R;
    float G;
    float B;
};

struct ThemeColors {
    Color ThemeColor0;
    Color ThemeColor1;
    Color ThemeColor2;
    Color ThemeColor3;
};

struct DFT {
    std::vector<float> mag;
    std::vector<float> magEQ;
    std::vector<float> magfilt;
    std::vector<float> magPhase;
};

/**
 * C++ wrapper struct for History with vectors
 */
struct History {
    std::vector<float> bass;
    std::vector<float> lowmid;
    std::vector<float> highmid;
    std::vector<float> treble;
};

struct FilteredAudiolink {
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
    DFT dft;
    FilteredAudiolink filtered_audiolink;
};
