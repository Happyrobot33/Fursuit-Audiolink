#pragma once

#include <vector>
#include "audiolink_data.pb.h"
#include "pb_decode.h"

/**
 * @class AudioProcessor
 * @brief Manages audio data reception and reconstruction from protobuf packets
 */
class AudioProcessor {
public:
    // Frequency data using vectors (auto-sized)
    std::vector<float> bass;
    std::vector<float> lowmid;
    std::vector<float> highmid;
    std::vector<float> treble;
    
    // Packet management
    std::vector<uint8_t> current_packet_data;
    std::vector<uint8_t> reconstructed_data;
    int packet_count;
    bool is_complete;
    Audiolink_Data audio_data;
    
    AudioProcessor();
    
    /**
     * Reset buffer and state for a new audio message
     */
    void reset();
    
    /**
     * Clear all frequency data vectors
     */
    void clear_frequency_data();
};

/**
 * Attempt to reconstruct and decode audio data from buffered packets
 * @param processor Pointer to AudioProcessor instance
 * @return true if reconstruction successful, false otherwise
 */
bool try_reconstruct_audio_data(AudioProcessor *processor);

/**
 * Callback for decoding Sub_Packet data field
 */
bool decode_subpacket_data_callback(pb_istream_t *stream, const pb_field_t *field, void **arg);

/**
 * Callbacks for decoding frequency bands
 */
bool decode_bass_callback(pb_istream_t *stream, const pb_field_t *field, void **arg);
bool decode_lowmid_callback(pb_istream_t *stream, const pb_field_t *field, void **arg);
bool decode_highmid_callback(pb_istream_t *stream, const pb_field_t *field, void **arg);
bool decode_treble_callback(pb_istream_t *stream, const pb_field_t *field, void **arg);
