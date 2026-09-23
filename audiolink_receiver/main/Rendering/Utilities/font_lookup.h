#pragma once

#include <cstdint>

/**
 * Minimal built-in 5x7 monospace bitmap font covering space through 'Z', used by
 * shaders that need to render text without loading external font assets.
 */
namespace FontLookup {

constexpr uint8_t GLYPH_WIDTH = 5;
constexpr uint8_t GLYPH_HEIGHT = 7;

/**
 * Returns true if pixel (col, row) within a GLYPH_WIDTH x GLYPH_HEIGHT character cell
 * is lit for `character`. col is measured left to right; row is measured bottom to top
 * (row 0 = the glyph's bottom scanline), matching the renderer's y-up UV convention so
 * callers never need to flip y themselves. Characters without a defined glyph (including
 * space) render blank.
 */
bool get_pixel(char character, uint8_t col, uint8_t row);

} // namespace FontLookup
