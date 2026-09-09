#pragma once

#include "shader.h"

/** Animated cellular/Voronoi pattern; all distance/hash math is done in Q16.16 fixed-point. */
class VoronoiShader : public IShader {
public:
    void begin_frame() override;
    Color render(float x, float y) override;

private:
    static constexpr uint8_t FEATURE_CACHE_SIZE = 10;
    static constexpr uint16_t GLOW_LUT_SIZE = 577;

    void update_feature_cache(uint32_t t);
    void initialize_glow_lut();

    int32_t feature_x_[FEATURE_CACHE_SIZE][FEATURE_CACHE_SIZE] = {};
    int32_t feature_y_[FEATURE_CACHE_SIZE][FEATURE_CACHE_SIZE] = {};
    uint8_t color_r_[FEATURE_CACHE_SIZE][FEATURE_CACHE_SIZE] = {};
    uint8_t color_g_[FEATURE_CACHE_SIZE][FEATURE_CACHE_SIZE] = {};
    uint8_t color_b_[FEATURE_CACHE_SIZE][FEATURE_CACHE_SIZE] = {};
    uint16_t glow_lut_[GLOW_LUT_SIZE] = {};
    bool glow_lut_initialized_ = false;
    uint32_t frame_time_ = 0;
};
