#pragma once

#include <vector>
#include <cstdint>
#include <string>

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

struct WaveForm {
    std::vector<float> wav1;
    std::vector<float> wav2;
    std::vector<float> wav3;
    std::vector<float> wav1diff;
};

enum class PlaybackState : int32_t {
    NONE = 0,
    PLAYING = 1,
    PAUSED = 2,
    STOPPED = 3,
    LOADING = 4,
    STREAMING = 5,
    ERROR = 6,
};

enum class LoopOrRandom : int32_t {
    NONE = 0,
    LOOP = 1,
    LOOP_ONE = 2,
    RANDOM = 3,
    RANDOM_AND_LOOP = 4,
};

struct MediaState {
    float mediaVolume = 0.0f;
    float mediaTime = 0.0f;
    PlaybackState mediaPlayback = PlaybackState::NONE;
    LoopOrRandom mediaLoop = LoopOrRandom::NONE;
};

struct PlayerData {
    int32_t numberOfPlayers = 0;
    bool isMaster = false;
    bool isOwner = false;
};

struct Intensity {
    float RMSLeft = 0.0f;
    float PeakLeft = 0.0f;
    float RMSRight = 0.0f;
    float PeakRight = 0.0f;
};

struct Autogain {
    float asymmetricGain = 0.0f;
    float symmetricGain = 0.0f;
};

struct GeneralVU {
    float versionMajor = 0.0f;
    float versionMinor = 0.0f;
    float systemFPS = 0.0f;
    float audioLinkFPS = 0.0f;
    double msSinceInstanceStart = 0.0;
    double msSinceMidnightLocal = 0.0;
    double msInNetworkTime = 0.0;
    MediaState media_state;
    PlayerData player_data;
    Intensity current_intensity;
    Intensity marker_value;
    Intensity marker_times;
    Autogain autogain;
    double UTCDaysSinceEpoch = 0.0;
    double msSinceUTCDayStart = 0.0;
};

struct ColorChord {
    std::vector<Color> colors;
    std::vector<Color> strip;
    std::vector<Color> lights_internal;
    std::vector<Color> lights;
};

struct AutoCorrelator {
    std::vector<float> autocorrelation;
    std::vector<float> uncorrelated;
};

struct ChronotensityBand {
    float increasing = 0.0f;
    float filtered_increasing = 0.0f;
    float bounce = 0.0f;
    float filtered_bounce = 0.0f;
    float intensity_pause = 0.0f;
    float filtered_intensity_pause = 0.0f;
    float bounce_pause = 0.0f;
    float filtered_bounce_pause = 0.0f;
};

struct Chronotensity {
    ChronotensityBand bass;
    ChronotensityBand lowmid;
    ChronotensityBand highmid;
    ChronotensityBand treble;
};

struct GlobalStrings {
    std::string playerName;
    std::string masterName;
    std::string customString1;
    std::string customString2;
};

/**
 * C++ wrapper struct for Audiolink_Data with vectors
 */
struct AudiolinkData {
    History history;
    ThemeColors theme_colors;
    DFT dft;
    FilteredAudiolink filtered_audiolink;
    WaveForm waveform;
    GeneralVU general_vu;
    ColorChord colorchord;
    AutoCorrelator autocorrelator;
    Chronotensity chronotensity;
    GlobalStrings global_strings;
};
