#include <cstring>
#include "esp_log.h"
#include "audio_processor.h"
#include "config.h"

AudioProcessor::AudioProcessor() : packet_count(0), is_complete(false) {
    memset(&audio_data, 0, sizeof(audio_data));
}

void AudioProcessor::reset() {
    packet_count = 0;
    is_complete = false;
    reconstructed_data.clear();
    current_packet_data.clear();
}

void AudioProcessor::clear_frequency_data() {
    bass.clear();
    lowmid.clear();
    highmid.clear();
    treble.clear();
}

bool decode_subpacket_data_callback(pb_istream_t *stream, const pb_field_t *field, void **arg) {
    std::vector<uint8_t> *buffer = static_cast<std::vector<uint8_t>*>(*arg);
    size_t bytes_to_read = stream->bytes_left;
    
    size_t old_size = buffer->size();
    buffer->resize(old_size + bytes_to_read);
    
    if (!pb_read(stream, buffer->data() + old_size, bytes_to_read)) {
        return false;
    }
    
    return true;
}

bool decode_bass_callback(pb_istream_t *stream, const pb_field_t *field, void **arg) {
    std::vector<float> *bass = static_cast<std::vector<float>*>(*arg);
    float value;
    if (!pb_decode_fixed32(stream, (uint32_t*)&value)) {
        return false;
    }
    bass->push_back(value);
    return true;
}

bool decode_lowmid_callback(pb_istream_t *stream, const pb_field_t *field, void **arg) {
    std::vector<float> *lowmid = static_cast<std::vector<float>*>(*arg);
    float value;
    if (!pb_decode_fixed32(stream, (uint32_t*)&value)) {
        return false;
    }
    lowmid->push_back(value);
    return true;
}

bool decode_highmid_callback(pb_istream_t *stream, const pb_field_t *field, void **arg) {
    std::vector<float> *highmid = static_cast<std::vector<float>*>(*arg);
    float value;
    if (!pb_decode_fixed32(stream, (uint32_t*)&value)) {
        return false;
    }
    highmid->push_back(value);
    return true;
}

bool decode_treble_callback(pb_istream_t *stream, const pb_field_t *field, void **arg) {
    std::vector<float> *treble = static_cast<std::vector<float>*>(*arg);
    float value;
    if (!pb_decode_fixed32(stream, (uint32_t*)&value)) {
        return false;
    }
    treble->push_back(value);
    return true;
}

bool try_reconstruct_audio_data(AudioProcessor *processor) {
    if (processor->reconstructed_data.empty()) {
        return false;
    }
    
    /* Clear previous frequency data */
    processor->clear_frequency_data();
    
    /* Set up callbacks for repeated float fields */
    processor->audio_data.history.bass.funcs.decode = decode_bass_callback;
    processor->audio_data.history.bass.arg = &processor->bass;
    
    processor->audio_data.history.lowmid.funcs.decode = decode_lowmid_callback;
    processor->audio_data.history.lowmid.arg = &processor->lowmid;
    
    processor->audio_data.history.highmid.funcs.decode = decode_highmid_callback;
    processor->audio_data.history.highmid.arg = &processor->highmid;
    
    processor->audio_data.history.treble.funcs.decode = decode_treble_callback;
    processor->audio_data.history.treble.arg = &processor->treble;
    
    /* Decode the concatenated data as Audiolink_Data using nanopb */
    pb_istream_t stream = pb_istream_from_buffer(processor->reconstructed_data.data(), 
                                               processor->reconstructed_data.size());
    bool status = pb_decode(&stream, Audiolink_Data_fields, &processor->audio_data);
    
    if (status) {
        processor->is_complete = true;
        ESP_LOGI(TAG, "Audio data reconstructed: received %d packets, bass=%zu, lowmid=%zu, highmid=%zu, treble=%zu",
                 processor->packet_count,
                 processor->bass.size(),
                 processor->lowmid.size(),
                 processor->highmid.size(),
                 processor->treble.size());
        return true;
    } else {
        ESP_LOGW(TAG, "Protobuf decode failed: %s", PB_GET_ERROR(&stream));
        return false;
    }
}
