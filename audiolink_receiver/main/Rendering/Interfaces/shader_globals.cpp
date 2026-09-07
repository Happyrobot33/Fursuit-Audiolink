#include "shader_globals.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

float _Time = 0.0f;

static AudiolinkData g_shader_audio_data;

const AudiolinkData &shader_audio_data() {
    return g_shader_audio_data;
}

void update_shader_globals(const AudiolinkData &audio_data) {
    _Time = static_cast<float>(xTaskGetTickCount()) * (static_cast<float>(portTICK_PERIOD_MS) / 1000.0f);
    g_shader_audio_data = audio_data;
}
