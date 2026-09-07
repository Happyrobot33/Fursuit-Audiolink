#include "color_utils.h"

#include <cmath>

void hsv_to_rgb(float hue, float saturation, float value, uint8_t *r, uint8_t *g, uint8_t *b) {
    float c = value * saturation;
    float x = c * (1 - std::fabs(std::fmod(hue / 60.0f, 2) - 1));
    float m = value - c;
    float rf, gf, bf;

    if (hue < 60) {
        rf = c; gf = x; bf = 0;
    } else if (hue < 120) {
        rf = x; gf = c; bf = 0;
    } else if (hue < 180) {
        rf = 0; gf = c; bf = x;
    } else if (hue < 240) {
        rf = 0; gf = x; bf = c;
    } else if (hue < 300) {
        rf = x; gf = 0; bf = c;
    } else {
        rf = c; gf = 0; bf = x;
    }

    *r = (uint8_t)((rf + m) * 255.0f);
    *g = (uint8_t)((gf + m) * 255.0f);
    *b = (uint8_t)((bf + m) * 255.0f);
}
