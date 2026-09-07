#pragma once

#include <cstdint>
#include "audiolink_data.h"

/**
 * @class IRenderTarget
 * @brief Abstracts a physical output device (LED matrix or LED strip) as a 2D pixel grid.
 *
 * Matrices expose their native width/height. LED strips are treated as a single row
 * (height() == 1), so callers should always divide UV coordinates by width()/height()
 * rather than assuming any fixed dimensions.
 */
class IRenderTarget {
public:
    virtual ~IRenderTarget() = default;
    virtual uint16_t width() const = 0;
    virtual uint16_t height() const = 0;
    virtual void set_pixel(uint16_t x, uint16_t y, const Color& color) = 0;
    virtual void clear() = 0;
    virtual void present() = 0;
};
