#include "renderer.h"

#include <algorithm>
#include <cmath>

#include "shader_config.h"
#if RENDER_PROFILING_ENABLED
#include "esp_timer.h"
#include "esp_log.h"
#endif
#include "shader_globals.h"

namespace {
#if RENDER_PROFILING_ENABLED
constexpr uint16_t PROFILE_SAMPLE_MASK = 0xFF;
constexpr uint32_t PROFILE_LOG_FRAMES = 30;

struct RenderProfile {
    uint32_t frames = 0;
    uint32_t samples = 0;
    int64_t frame_us = 0;
    int64_t clear_us = 0;
    int64_t uv_us = 0;
    int64_t shader_us = 0;
    int64_t output_us = 0;
    int64_t present_us = 0;
};

RenderProfile render_profile;

void log_render_profile() {
    if (render_profile.frames < PROFILE_LOG_FRAMES) {
        return;
    }

    const float frames = static_cast<float>(render_profile.frames);
    const float samples = static_cast<float>(render_profile.samples);
    ESP_LOGI("render_profile",
             "avg %.2fms/frame (%.2f FPS) | clear %.2fms | sampled uv %.2fus, shader %.2fus, output %.2fus | present %.2fms | samples/frame %.1f",
             static_cast<float>(render_profile.frame_us) / frames / 1000.0f,
             1000000.0f / (static_cast<float>(render_profile.frame_us) / frames),
             static_cast<float>(render_profile.clear_us) / frames / 1000.0f,
             samples > 0.0f ? static_cast<float>(render_profile.uv_us) / samples : 0.0f,
             samples > 0.0f ? static_cast<float>(render_profile.shader_us) / samples : 0.0f,
             samples > 0.0f ? static_cast<float>(render_profile.output_us) / samples : 0.0f,
             static_cast<float>(render_profile.present_us) / frames / 1000.0f,
             samples / frames);
    render_profile = {};
}
#endif

bool apply_repeat_mode(float& u, float& v, UvRepeatMode repeat_mode) {
    switch (repeat_mode) {
        case UvRepeatMode::Blank:
            return u >= 0.0f && u <= 1.0f && v >= 0.0f && v <= 1.0f;
        case UvRepeatMode::Flip: {
            const auto flip = [](float value) {
                const float wrapped = std::fmod(value, 2.0f);
                const float positive = wrapped < 0.0f ? wrapped + 2.0f : wrapped;
                return positive <= 1.0f ? positive : 2.0f - positive;
            };
            u = flip(u);
            v = flip(v);
            return true;
        }
        case UvRepeatMode::Repeat:
            u = std::fmod(u, 1.0f);
            v = std::fmod(v, 1.0f);
            if (u < 0.0f) {
                u += 1.0f;
            }
            if (v < 0.0f) {
                v += 1.0f;
            }
            return true;
    }
    return false;
}

float normalized_coordinate(uint16_t coordinate, uint16_t size) {
    return size > 0 ? (static_cast<float>(coordinate) + 0.5f) / static_cast<float>(size) : 0.0f;
}

float flipped_v(float v) {
    const float flipped = 1.0f - v;
    return flipped >= 1.0f ? std::nextafter(1.0f, 0.0f) : flipped;
}

float alignment_offset(float domain_size, float content_size, UvAlignment alignment) {
    const float padding = std::max(0.0f, domain_size - content_size);
    switch (alignment) {
        case UvAlignment::Left:
            return 0.0f;
        case UvAlignment::Center:
            return padding * 0.5f;
        case UvAlignment::Right:
            return padding;
    }
    return 0.0f;
}

float tile_offset(float tile_repeat, UvAlignment alignment) {
    switch (alignment) {
        case UvAlignment::Left:
            return 0.0f;
        case UvAlignment::Center:
            return (1.0f - tile_repeat) * 0.5f;
        case UvAlignment::Right:
            return 1.0f - tile_repeat;
    }
    return 0.0f;
}

bool compute_uv(float& u, float& v, uint16_t x, uint16_t y, uint16_t w, uint16_t h, UvFitMode fit_mode,
                UvAlignment alignment, UvRepeatMode repeat_mode) {
    if (fit_mode == UvFitMode::Stretch) {
        u = normalized_coordinate(x, w);
        v = normalized_coordinate(y, h);
        return apply_repeat_mode(u, v, repeat_mode);
    }

    const float domain_width = 1.0f;
    const float domain_height = 1.0f;
    const bool wide = w >= h;
    const float contained_scale = wide ? static_cast<float>(h) / static_cast<float>(w)
                                      : static_cast<float>(w) / static_cast<float>(h);
    const float tile_repeat = wide ? static_cast<float>(w) / static_cast<float>(h)
                                  : static_cast<float>(h) / static_cast<float>(w);

    if (fit_mode == UvFitMode::Tile) {
        const float repeat_u = normalized_coordinate(x, w);
        const float repeat_v = normalized_coordinate(y, h);
        if (wide) {
            const float offset = tile_offset(tile_repeat, alignment);
            u = repeat_u * tile_repeat + offset;
            v = repeat_v;
        } else {
            const float offset = tile_offset(tile_repeat, alignment);
            u = repeat_u;
            v = repeat_v * tile_repeat + offset;
        }
        return apply_repeat_mode(u, v, repeat_mode);
    }

    // Cover: fill the larger axis and crop the smaller axis.
    if (wide) {
        u = normalized_coordinate(x, w);
        v = normalized_coordinate(y, h) * contained_scale;
        v += alignment_offset(domain_height, contained_scale, alignment) / domain_height;
    } else {
        u = normalized_coordinate(x, w) * contained_scale;
        v = normalized_coordinate(y, h);
        u += alignment_offset(domain_width, contained_scale, alignment) / domain_width;
    }
    return apply_repeat_mode(u, v, repeat_mode);
}
}

void render_shader_frame(IRenderTarget& target, IShader& shader, const AudiolinkData& audio_data,
                         UvFitMode fit_mode, UvAlignment alignment, UvRepeatMode repeat_mode) {
    const uint16_t w = target.width();
    const uint16_t h = target.height();
    if (w == 0 || h == 0) {
        return;
    }

#if RENDER_PROFILING_ENABLED
    const int64_t frame_start = esp_timer_get_time();
#endif
    update_shader_globals(audio_data);
    shader.begin_frame();

#if RENDER_PROFILING_ENABLED
    int64_t stage_start = esp_timer_get_time();
#endif
    target.clear();
#if RENDER_PROFILING_ENABLED
    render_profile.clear_us += esp_timer_get_time() - stage_start;
#endif

    for (uint16_t y = 0; y < h; ++y) {
        for (uint16_t x = 0; x < w; ++x) {
#if RENDER_PROFILING_ENABLED
            const bool sample = ((static_cast<uint32_t>(y) * w + x) & PROFILE_SAMPLE_MASK) == 0;
#endif
            float u = 0.0f;
            float v = 0.0f;
#if RENDER_PROFILING_ENABLED
            if (sample) {
                stage_start = esp_timer_get_time();
            }
#endif
            if (!compute_uv(u, v, x, y, w, h, fit_mode, alignment, repeat_mode)) {
                continue;
            }
#if RENDER_PROFILING_ENABLED
            if (sample) {
                render_profile.uv_us += esp_timer_get_time() - stage_start;
                stage_start = esp_timer_get_time();
            }
#endif
            // V coordinate is flipped here to fix UV layout.
            const Color color = shader.render(u, flipped_v(v));
#if RENDER_PROFILING_ENABLED
            if (sample) {
                render_profile.shader_us += esp_timer_get_time() - stage_start;
                stage_start = esp_timer_get_time();
            }
#endif
            target.set_pixel(x, y, color);
#if RENDER_PROFILING_ENABLED
            if (sample) {
                render_profile.output_us += esp_timer_get_time() - stage_start;
                render_profile.samples++;
            }
#endif
        }
    }

#if RENDER_PROFILING_ENABLED
    stage_start = esp_timer_get_time();
#endif
    target.present();
#if RENDER_PROFILING_ENABLED
    render_profile.present_us += esp_timer_get_time() - stage_start;
    render_profile.frames++;
    render_profile.frame_us += esp_timer_get_time() - frame_start;
    log_render_profile();
#endif
}

void render_shader_frame(IRenderTarget& target, IShader& shader, const AudiolinkData& audio_data, UvFitMode fit_mode) {
    const uint16_t w = target.width();
    const uint16_t h = target.height();
    if (w == 0 || h == 0) {
        return;
    }

#if RENDER_PROFILING_ENABLED
    const int64_t frame_start = esp_timer_get_time();
#endif
    update_shader_globals(audio_data);
    shader.begin_frame();

#if RENDER_PROFILING_ENABLED
    int64_t stage_start = esp_timer_get_time();
#endif
    target.clear();
#if RENDER_PROFILING_ENABLED
    render_profile.clear_us += esp_timer_get_time() - stage_start;
#endif

    for (uint16_t y = 0; y < h; ++y) {
        for (uint16_t x = 0; x < w; ++x) {
#if RENDER_PROFILING_ENABLED
            const bool sample = ((static_cast<uint32_t>(y) * w + x) & PROFILE_SAMPLE_MASK) == 0;
#endif
            float u = 0.0f;
            float v = 0.0f;
#if RENDER_PROFILING_ENABLED
            if (sample) {
                stage_start = esp_timer_get_time();
            }
#endif
            if (!compute_uv(u, v, x, y, w, h, fit_mode, UvAlignment::Center, UvRepeatMode::Repeat)) {
                continue;
            }
#if RENDER_PROFILING_ENABLED
            if (sample) {
                render_profile.uv_us += esp_timer_get_time() - stage_start;
                stage_start = esp_timer_get_time();
            }
#endif
            // V coordinate is flipped here to fix UV layout.
            const Color color = shader.render(u, flipped_v(v));
#if RENDER_PROFILING_ENABLED
            if (sample) {
                render_profile.shader_us += esp_timer_get_time() - stage_start;
                stage_start = esp_timer_get_time();
            }
#endif
            target.set_pixel(x, y, color);
#if RENDER_PROFILING_ENABLED
            if (sample) {
                render_profile.output_us += esp_timer_get_time() - stage_start;
                render_profile.samples++;
            }
#endif
        }
    }

#if RENDER_PROFILING_ENABLED
    stage_start = esp_timer_get_time();
#endif
    target.present();
#if RENDER_PROFILING_ENABLED
    render_profile.present_us += esp_timer_get_time() - stage_start;
    render_profile.frames++;
    render_profile.frame_us += esp_timer_get_time() - frame_start;
    log_render_profile();
#endif
}
