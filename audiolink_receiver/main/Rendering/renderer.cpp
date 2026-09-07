#include "renderer.h"

#include "shader_globals.h"

void render_shader_frame(IRenderTarget& target, IShader& shader, const AudiolinkData& audio_data) {
    const uint16_t w = target.width();
    const uint16_t h = target.height();
    if (w == 0 || h == 0) {
        return;
    }

    update_shader_globals(audio_data);

    target.clear();
    for (uint16_t y = 0; y < h; ++y) {
        // Strips report height 1; keep v pinned to 0 instead of dividing by (h - 1).
        const float v = (h > 1) ? static_cast<float>(y) / static_cast<float>(h - 1) : 0.0f;
        for (uint16_t x = 0; x < w; ++x) {
            const float u = (w > 1) ? static_cast<float>(x) / static_cast<float>(w - 1) : 0.0f;
            target.set_pixel(x, y, shader.render(u, v));
        }
    }
    target.present();
}
