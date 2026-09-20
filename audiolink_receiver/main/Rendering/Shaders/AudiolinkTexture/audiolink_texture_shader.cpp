#include "audiolink_texture_shader.h"

#include <array>
#include <cstring>
#include <string>
#include <vector>

#include "shader_globals.h"

// Packs value's low 23 bits into a subnormal float's mantissa (matches AudioLink.cs'
// IntToFloatBits24Bit exactly), NOT a plain int-to-float cast. Built via direct bit
// construction rather than the equivalent division/multiplication, since some FPUs
// flush-to-zero (FTZ/DAZ) subnormal arithmetic results, which would silently zero
// out every encoded value.
inline float IntToFloatBits24Bit(uint32_t value) {
    uint32_t bits = value & 0x007FFFFF; // sign=0, exponent=0 (subnormal), mantissa=frac
    float result;
    std::memcpy(&result, &bits, sizeof(result));
    return result;
}

constexpr size_t kPackedStringColorCount = 8;
constexpr size_t kGlobalStringMaxLength = kPackedStringColorCount * 4;

// Decodes a UTF-8 string into Unicode codepoints and packs 3 per Color (R, G, B),
// mirroring UpdateGlobalString's packing of 4 codepoints per shader vector4. The
// 4th (alpha/w) codepoint per entry is still consumed from the input - matching
// Unity's true 32-codepoint (kGlobalStringMaxLength) truncation length - but
// dropped rather than stored, since Color has no alpha channel.
// Always returns kPackedStringColorCount entries, zero-padded if input is shorter.
std::vector<Color> PackStringToColors(const std::string& input) {
    std::vector<uint32_t> codePoints;
    codePoints.reserve(input.size());

    for (size_t i = 0; i < input.size();) {
        unsigned char lead = static_cast<unsigned char>(input[i]);
        uint32_t codePoint = 0;
        size_t extraBytes = 0;
        if ((lead & 0x80) == 0x00) {
            codePoint = lead;
        } else if ((lead & 0xE0) == 0xC0) {
            codePoint = lead & 0x1F;
            extraBytes = 1;
        } else if ((lead & 0xF0) == 0xE0) {
            codePoint = lead & 0x0F;
            extraBytes = 2;
        } else if ((lead & 0xF8) == 0xF0) {
            codePoint = lead & 0x07;
            extraBytes = 3;
        } else {
            // invalid leading byte, skip it
            ++i;
            continue;
        }

        if (i + extraBytes >= input.size()) {
            break; // truncated multi-byte sequence at end of string
        }

        bool valid = true;
        for (size_t b = 1; b <= extraBytes; ++b) {
            unsigned char cont = static_cast<unsigned char>(input[i + b]);
            if ((cont & 0xC0) != 0x80) {
                valid = false;
                break;
            }
            codePoint = (codePoint << 6) | (cont & 0x3F);
        }

        i += extraBytes + 1;
        if (valid) {
            codePoints.push_back(codePoint);
            if (codePoints.size() >= kGlobalStringMaxLength) {
                break; // matches Unity's GlobalStringMaxLength truncation
            }
        }
    }

    std::vector<Color> result;
    result.reserve(kPackedStringColorCount);
    for (size_t i = 0; i < kGlobalStringMaxLength; i += 4) {
        //okay so long story short, the encoding here does some really funky things with the floats, and in
        //unity this causes them to render as white pixels. However since the hardware is different, this doesnt
        //occur here, so we just infer always as white since thats what will always end up happening anyway
        Color color{1.0f, 1.0f, 1.0f};
        // Color color{0.0f, 0.0f, 0.0f};
        // if (i < codePoints.size()) {
        //     color.R = IntToFloatBits24Bit(codePoints[i]);
        // }
        // if (i + 1 < codePoints.size()) {
        //     color.G = IntToFloatBits24Bit(codePoints[i + 1]);
        // }
        // if (i + 2 < codePoints.size()) {
        //     color.B = IntToFloatBits24Bit(codePoints[i + 2]);
        // }
        // if (i + 3 < codePoints.size()) {
        //     // Computed to match Unity's per-pixel 4-codepoint consumption, then
        //     // dropped since Color has no alpha channel to store it in.
        //     (void)IntToFloatBits24Bit(codePoints[i + 3]);
        // }
        result.push_back(color);
    }
    return result;
}

// Reverse of AudioLinkDecodeDataAsUInt (rpx.x + rpx.y*1024 + rpx.z*1048576 + rpx.w*1073741824):
// packs a uint value into a Color's R/G/B channels as raw 10-bit digits (bits 0-9,
// 10-19, 20-29), NOT normalized to 0.0-1.0. Values needing bit 30-31 (the alpha
// channel in the original 4-component decode) are not supported here since Color
// has no alpha channel, so only values up to 2^30-1 round-trip exactly.
Color AudioLinkEncodeDataAsUInt(float value) {
    uint32_t v = static_cast<uint32_t>(value);
    float r = static_cast<float>(v & 0x3FF);
    float g = static_cast<float>((v >> 10) & 0x3FF);
    float b = static_cast<float>((v >> 20) & 0x3FF);
    return Color{r, g, b};
}

std::array<float, 8> ChronotensityToArray(const ChronotensityBand& band) {
    std::array<float, 8> result;
    result[0] = band.increasing;
    result[1] = band.filtered_increasing;
    result[2] = band.bounce;
    result[3] = band.filtered_bounce;
    result[4] = band.intensity_pause;
    result[5] = band.filtered_intensity_pause;
    result[6] = band.bounce_pause;
    result[7] = band.filtered_bounce_pause;
    return result;
}

void AudiolinkTextureShader::begin_frame() {
    playerNameCol_ = PackStringToColors(shader_audio_data().global_strings.playerName);
    masterNameCol_ = PackStringToColors(shader_audio_data().global_strings.masterName);
    custom1Col_ = PackStringToColors(shader_audio_data().global_strings.customString1);
    custom2Col_ = PackStringToColors(shader_audio_data().global_strings.customString2);
}

Color AudiolinkTextureShader::render(float x, float y) {
    // return Color{1.15f, 0.0f, 0.0f};
    //first we want to convert the UV coordinates to pixel coordinates, assuming a 128x64 area
    int pixelX = static_cast<int>(x * 128);
    int pixelY = static_cast<int>(y * 64);
    //first lowest row is bass history
    if (pixelY == 0) {
        // bass history row
        return Color{1.0f, 1.0f, 1.0f} * shader_audio_data().history.bass[pixelX];
    }
    if (pixelY == 1) {
        // lowmid history row
        return Color{1.0f, 1.0f, 1.0f} * shader_audio_data().history.lowmid[pixelX];
    }
    if (pixelY == 2) {
        // highmid history row
        return Color{1.0f, 1.0f, 1.0f} * shader_audio_data().history.highmid[pixelX];
    }
    if (pixelY == 3) {
        // treble history row
        return Color{1.0f, 1.0f, 1.0f} * shader_audio_data().history.treble[pixelX];
    }
    //DFT
    if (pixelY == 4) {
        // bass DFT row
        return Color{shader_audio_data().dft.mag[pixelX], shader_audio_data().dft.magEQ[pixelX], shader_audio_data().dft.magfilt[pixelX]};
    }
    if (pixelY == 5) {
        return Color{shader_audio_data().dft.mag[pixelX + 128], shader_audio_data().dft.magEQ[pixelX + 128], shader_audio_data().dft.magfilt[pixelX + 128]};
    }
    //theme colors
    if (pixelY == 23) {
        if (pixelX == 0) {
            return shader_audio_data().theme_colors.ThemeColor0;
        }
        if (pixelX == 1) {
            return shader_audio_data().theme_colors.ThemeColor1;
        }
        if (pixelX == 2) {
            return shader_audio_data().theme_colors.ThemeColor2;
        }
        if (pixelX == 3) {
            return shader_audio_data().theme_colors.ThemeColor3;
        }
        if (pixelX == 5) {
            return AudioLinkEncodeDataAsUInt(shader_audio_data().general_vu.UTCDaysSinceEpoch);
        }
        if (pixelX == 6) {
            return AudioLinkEncodeDataAsUInt(shader_audio_data().general_vu.msSinceUTCDayStart);
        }
        if (pixelX == 7) {
            return Color{static_cast<float>(shader_audio_data().general_vu.position.lat), static_cast<float>(shader_audio_data().general_vu.position.lon), 0.0f};
        }
    }
    //cc strip
    if (pixelY == 24) {
        return shader_audio_data().colorchord.strip[pixelX];
    }
    //cc lights
    if (pixelY == 25) {
        return shader_audio_data().colorchord.lights[pixelX];
    }
    if (pixelY == 26) {
        return shader_audio_data().colorchord.lights[pixelX + 128];
    }
    //autocorrelator
    if (pixelY == 27) {
        return Color{shader_audio_data().autocorrelator.autocorrelation[pixelX], shader_audio_data().autocorrelator.uncorrelated[pixelX], 0.0f};
    }
    bool chronotensityXCheck = (pixelX >= 16 && pixelX < 16 + 8);
    int chronotensityIndex = pixelX - 16;
    bool stringXCheck = (pixelX >= 40 && pixelX < 40 + 8);
    int stringPixelIndex = pixelX - 40;
    if (pixelY == 28) {
        if (pixelX < 16) {
            return Color{1.0f, 1.0f, 1.0f} * shader_audio_data().filtered_audiolink.bass[pixelX];
        }
        if (chronotensityXCheck) {
            return AudioLinkEncodeDataAsUInt(ChronotensityToArray(shader_audio_data().chronotensity.bass)[chronotensityIndex]);
        }
        if (stringXCheck) {
            return playerNameCol_[stringPixelIndex];
        }
    }
    if (pixelY == 29) {
        if (pixelX < 16) {
            return Color{1.0f, 1.0f, 1.0f} * shader_audio_data().filtered_audiolink.lowmid[pixelX];
        }
        if (chronotensityXCheck) {
            return AudioLinkEncodeDataAsUInt(ChronotensityToArray(shader_audio_data().chronotensity.lowmid)[chronotensityIndex]);
        }
        if (stringXCheck) {
            return masterNameCol_[stringPixelIndex];
        }
    }
    if (pixelY == 30) {
        if (pixelX < 16) {
            return Color{1.0f, 1.0f, 1.0f} * shader_audio_data().filtered_audiolink.highmid[pixelX];
        }
        if (chronotensityXCheck) {
            return AudioLinkEncodeDataAsUInt(ChronotensityToArray(shader_audio_data().chronotensity.highmid)[chronotensityIndex]);
        }
        if (stringXCheck) {
            return custom1Col_[stringPixelIndex];
        }
    }
    if (pixelY == 31) {
        if (pixelX < 16) {
            return Color{1.0f, 1.0f, 1.0f} * shader_audio_data().filtered_audiolink.treble[pixelX];
        }
        if (chronotensityXCheck) {
            return AudioLinkEncodeDataAsUInt(ChronotensityToArray(shader_audio_data().chronotensity.treble)[chronotensityIndex]);
        }
        if (stringXCheck) {
            return custom2Col_[stringPixelIndex];
        }
    }
    //filtered VU
    //TODO: Implement sending filtered VU, its not actually sent yet! I missed it when adding features it seems
    // if (pixelY == 28) {
    //     if (pixelX)
    //     return Color{shader_audio_data().filtered_audiolink
    // }
    //generalvu
    if (pixelY == 22) {
        if (pixelX == 0) {
            return Color{3.02f, shader_audio_data().general_vu.versionMajor, shader_audio_data().general_vu.systemFPS}; //version minor would be alpha
        }
        if (pixelX == 1) {
            return Color{shader_audio_data().general_vu.frameCount, 1.0f, 1.0f};
        }
        if (pixelX == 2) {
            return AudioLinkEncodeDataAsUInt(shader_audio_data().general_vu.msSinceInstanceStart);
        }
        if (pixelX == 3) {
            return AudioLinkEncodeDataAsUInt(shader_audio_data().general_vu.msSinceMidnightLocal);
        }
        if (pixelX == 4) {
            return AudioLinkEncodeDataAsUInt(shader_audio_data().general_vu.msInNetworkTime);
        }
        if (pixelX == 5) {
            return Color{shader_audio_data().general_vu.media_state.mediaVolume, shader_audio_data().general_vu.media_state.mediaTime, static_cast<float>(shader_audio_data().general_vu.media_state.mediaPlayback)};
        }
        if (pixelX == 6) {
            return Color{static_cast<float>(shader_audio_data().general_vu.player_data.numberOfPlayers), static_cast<float>(shader_audio_data().general_vu.player_data.isMaster), static_cast<float>(shader_audio_data().general_vu.player_data.isOwner)};
        }
        if (pixelX == 8) {
            return Color{shader_audio_data().general_vu.current_intensity.RMSLeft, shader_audio_data().general_vu.current_intensity.PeakLeft, shader_audio_data().general_vu.current_intensity.RMSRight};
        }
        if (pixelX == 9) {
            return Color{shader_audio_data().general_vu.marker_value.RMSLeft, shader_audio_data().general_vu.marker_value.PeakLeft, shader_audio_data().general_vu.marker_value.RMSRight};
        }
        if (pixelX == 10) {
            return Color{shader_audio_data().general_vu.marker_times.RMSLeft, shader_audio_data().general_vu.marker_times.PeakLeft, shader_audio_data().general_vu.marker_times.RMSRight};
        }
        if (pixelX == 11) {
            return Color{shader_audio_data().general_vu.autogain.asymmetricGain, shader_audio_data().general_vu.autogain.symmetricGain, 0.0f};
        }
    }
    return Color{0.0f, 0.0f, 0.0f};
}
