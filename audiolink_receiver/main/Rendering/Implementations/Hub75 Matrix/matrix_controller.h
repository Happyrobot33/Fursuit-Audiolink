#pragma once

#include <vector>

#include "esp_err.h"
#include "audiolink_data.h"

class MatrixController {
public:
	esp_err_t init();
	void deinit();
	void render_dft(const std::vector<float>& magnitudes);
	void render_sample_pattern();
	void fill(const Color& color);

	// Pixel-level access used by the IRenderTarget abstraction.
	void set_pixel(uint16_t x, uint16_t y, const Color& color);
	void clear();
	void present();

private:
	class MatrixPanel_I2S_DMA* driver_ = nullptr;
};
