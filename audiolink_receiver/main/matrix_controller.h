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

private:
	class MatrixPanel_I2S_DMA* driver_ = nullptr;
};
