#include "text_shader.h"

#include <cmath>
#include <cstdio>

#include "color_utils.h"
#include "config.h"
#include "font_lookup.h"
#include "shader_globals.h"

namespace {
constexpr const char *FIRST_LINE = "NO SIGNAL";
constexpr int FIRST_LINE_LENGTH = sizeof("NO SIGNAL") - 1;
constexpr int SECOND_LINE_LENGTH = 9; // "MM:SS.mmm"

constexpr uint8_t SCALE = 1;         // pixel scale of each glyph
constexpr uint8_t CHAR_SPACING = 1;  // gap between glyphs, in unscaled pixels
constexpr uint8_t LINE_SPACING = 2;  // gap between the two lines, in unscaled pixels

constexpr int GLYPH_STRIDE = (FontLookup::GLYPH_WIDTH + CHAR_SPACING) * SCALE;
constexpr int LINE_HEIGHT = FontLookup::GLYPH_HEIGHT * SCALE;
constexpr int LINE_GAP = LINE_SPACING * SCALE;
constexpr int BLOCK_HEIGHT = LINE_HEIGHT * 2 + LINE_GAP;
constexpr int BLOCK_ORIGIN_Y = (SCREEN_HEIGHT - BLOCK_HEIGHT) / 2;

constexpr float HUE_DEGREES_PER_SECOND = 60.0f; // full rainbow cycle every 6 seconds

int line_width_px(int char_count) {
    return char_count * GLYPH_STRIDE - CHAR_SPACING * SCALE;
}

// Returns true if pixel (local_x, local_y) is lit for `text`, horizontally centered on screen.
bool render_line(const char *text, int char_count, int local_x, int local_y) {
    const int origin_x = (SCREEN_WIDTH - line_width_px(char_count)) / 2;
    const int x = local_x - origin_x;
    if (x < 0) {
        return false;
    }

    const int glyph_index = x / GLYPH_STRIDE;
    const int glyph_local_x = x % GLYPH_STRIDE;
    if (glyph_index >= char_count || glyph_local_x >= FontLookup::GLYPH_WIDTH * SCALE) {
        return false; // past the message, or in the inter-glyph gap
    }

    const uint8_t col = static_cast<uint8_t>(glyph_local_x / SCALE);
    const uint8_t row = static_cast<uint8_t>(local_y / SCALE);
    return FontLookup::get_pixel(text[glyph_index], col, row);
}
} // namespace

void TextShader::begin_frame() {
    // Clamp to non-negative so GCC can prove minutes/seconds/milliseconds never need a sign digit.
    const unsigned int total_ms = static_cast<unsigned int>(_Time > 0.0f ? _Time * 1000.0f : 0.0f);
    const unsigned int minutes = (total_ms / 60000) % 100;
    const unsigned int seconds = (total_ms / 1000) % 60;
    const unsigned int milliseconds = total_ms % 1000;
    std::snprintf(second_line_, sizeof(second_line_), "%02u:%02u.%03u", minutes, seconds, milliseconds);

    const float hue = std::fmod(_Time * HUE_DEGREES_PER_SECOND, 360.0f);
    uint8_t r, g, b;
    hsv_to_rgb(hue, 1.0f, 1.0f, &r, &g, &b);
    second_line_color_ = Color{r / 255.0f, g / 255.0f, b / 255.0f};
}

Color TextShader::render(float x, float y) {
    const int screen_x = static_cast<int>(x * SCREEN_WIDTH);
    const int screen_y = static_cast<int>(y * SCREEN_HEIGHT); // 0 = bottom of screen

    const int local_y = screen_y - BLOCK_ORIGIN_Y;
    if (local_y < 0 || local_y >= BLOCK_HEIGHT) {
        return Color{0.0f, 0.0f, 0.0f};
    }

    // Bottom-up block layout: the second line sits at the bottom, "NO SIGNAL" on top.
    if (local_y < LINE_HEIGHT) {
        const bool lit = render_line(second_line_, SECOND_LINE_LENGTH, screen_x, local_y);
        return lit ? second_line_color_ : Color{0.0f, 0.0f, 0.0f};
    }
    if (local_y >= LINE_HEIGHT + LINE_GAP) {
        const bool lit = render_line(FIRST_LINE, FIRST_LINE_LENGTH, screen_x, local_y - LINE_HEIGHT - LINE_GAP);
        return lit ? Color{1.0f, 1.0f, 1.0f} : Color{0.0f, 0.0f, 0.0f};
    }

    return Color{0.0f, 0.0f, 0.0f}; // inter-line gap
}
