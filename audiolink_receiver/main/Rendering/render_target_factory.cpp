#include "render_target_factory.h"

#include "esp_err.h"
#include "matrix_controller.h"
#include "led_controller.h"
#include "matrix_render_target.h"
#include "led_strip_render_target.h"
#include "matrix_strip_render_target.h"
#include "config.h"

std::unique_ptr<IRenderTarget> create_render_target() {
    if constexpr (ACTIVE_OUTPUT_DEVICE == OutputDevice::Matrix) {
        static MatrixController matrix;
        ESP_ERROR_CHECK(matrix.init());
        return std::make_unique<MatrixRenderTarget>(matrix);
    } else if constexpr (ACTIVE_OUTPUT_DEVICE == OutputDevice::MatrixStrip) {
        static LEDController led;
        led_strip_handle_t strip = led.init();
        return std::make_unique<MatrixStripRenderTarget>(led, strip);
    } else {
        static LEDController led;
        led_strip_handle_t strip = led.init();
        return std::make_unique<LedStripRenderTarget>(led, strip);
    }
}
