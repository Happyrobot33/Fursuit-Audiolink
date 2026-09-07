#pragma once

#include <cstdint>

/**
 * Convert HSV color to RGB
 * @param hue Hue angle (0-360)
 * @param saturation Saturation (0.0-1.0)
 * @param value Value/Brightness (0.0-1.0)
 * @param r Output red channel
 * @param g Output green channel
 * @param b Output blue channel
 */
void hsv_to_rgb(float hue, float saturation, float value,
                uint8_t *r, uint8_t *g, uint8_t *b);
