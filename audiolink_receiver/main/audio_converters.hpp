#pragma once

#include <vector>
#include "audiolink_data.h"
#include "audiolink_data.pb.h"
#include "nanopb_cpp.h"

using namespace NanoPb::Converter;

class ColorConverter : public MessageConverter<
        ColorConverter,
        Color,
        PROTO_Color,
        &PROTO_Color_msg>
{
public:
    static ProtoType encoderInit(const LocalType& local) {
        return ProtoType{
                .R = local.R,
                .G = local.G,
                .B = local.B
        };
    }

    static ProtoType decoderInit(LocalType& local) {
        return ProtoType{
                .R = local.R,
                .G = local.G,
                .B = local.B
        };
    }

    static bool decoderApply(const ProtoType& proto, LocalType& local) {
        local.R = proto.R;
        local.G = proto.G;
        local.B = proto.B;
        return true;
    }
};

class PlaybackStateConverter : public EnumConverter<
        PlaybackStateConverter,
        PlaybackState,
        PROTO_PlaybackState>
{
public:
    static ProtoType encode(const LocalType &local) {
        switch (local) {
            case PlaybackState::PLAYING:
                return PROTO_PlaybackState_PLAYBACK_STATE_PLAYING;
            case PlaybackState::PAUSED:
                return PROTO_PlaybackState_PLAYBACK_STATE_PAUSED;
            case PlaybackState::STOPPED:
                return PROTO_PlaybackState_PLAYBACK_STATE_STOPPED;
            case PlaybackState::LOADING:
                return PROTO_PlaybackState_PLAYBACK_STATE_LOADING;
            case PlaybackState::STREAMING:
                return PROTO_PlaybackState_PLAYBACK_STATE_STREAMING;
            case PlaybackState::ERROR:
                return PROTO_PlaybackState_PLAYBACK_STATE_ERROR;
            case PlaybackState::NONE:
            default:
                return PROTO_PlaybackState_PLAYBACK_STATE_NONE;
        }
    }

    static LocalType decode(const ProtoType &proto) {
        switch (proto) {
            case PROTO_PlaybackState_PLAYBACK_STATE_PLAYING:
                return PlaybackState::PLAYING;
            case PROTO_PlaybackState_PLAYBACK_STATE_PAUSED:
                return PlaybackState::PAUSED;
            case PROTO_PlaybackState_PLAYBACK_STATE_STOPPED:
                return PlaybackState::STOPPED;
            case PROTO_PlaybackState_PLAYBACK_STATE_LOADING:
                return PlaybackState::LOADING;
            case PROTO_PlaybackState_PLAYBACK_STATE_STREAMING:
                return PlaybackState::STREAMING;
            case PROTO_PlaybackState_PLAYBACK_STATE_ERROR:
                return PlaybackState::ERROR;
            case PROTO_PlaybackState_PLAYBACK_STATE_NONE:
            default:
                return PlaybackState::NONE;
        }
    }
};

class LoopOrRandomConverter : public EnumConverter<
        LoopOrRandomConverter,
        LoopOrRandom,
        PROTO_LoopOrRandom>
{
public:
    static ProtoType encode(const LocalType &local) {
        switch (local) {
            case LoopOrRandom::LOOP:
                return PROTO_LoopOrRandom_LOOP_OR_RANDOM_LOOP;
            case LoopOrRandom::LOOP_ONE:
                return PROTO_LoopOrRandom_LOOP_OR_RANDOM_LOOP_ONE;
            case LoopOrRandom::RANDOM:
                return PROTO_LoopOrRandom_LOOP_OR_RANDOM_RANDOM;
            case LoopOrRandom::RANDOM_AND_LOOP:
                return PROTO_LoopOrRandom_LOOP_OR_RANDOM_RANDOM_AND_LOOP;
            case LoopOrRandom::NONE:
            default:
                return PROTO_LoopOrRandom_LOOP_OR_RANDOM_NONE;
        }
    }

    static LocalType decode(const ProtoType &proto) {
        switch (proto) {
            case PROTO_LoopOrRandom_LOOP_OR_RANDOM_LOOP:
                return LoopOrRandom::LOOP;
            case PROTO_LoopOrRandom_LOOP_OR_RANDOM_LOOP_ONE:
                return LoopOrRandom::LOOP_ONE;
            case PROTO_LoopOrRandom_LOOP_OR_RANDOM_RANDOM:
                return LoopOrRandom::RANDOM;
            case PROTO_LoopOrRandom_LOOP_OR_RANDOM_RANDOM_AND_LOOP:
                return LoopOrRandom::RANDOM_AND_LOOP;
            case PROTO_LoopOrRandom_LOOP_OR_RANDOM_NONE:
            default:
                return LoopOrRandom::NONE;
        }
    }
};

class ThemeColorsConverter : public MessageConverter<
        ThemeColorsConverter,
        ThemeColors,
        PROTO_ThemeColors,
        &PROTO_ThemeColors_msg>
{
public:
    static ProtoType encoderInit(const LocalType& local) {
        return ProtoType{
                .has_ThemeColor0 = true,
                .ThemeColor0 = ColorConverter::encoderInit(local.ThemeColor0),
                .has_ThemeColor1 = true,
                .ThemeColor1 = ColorConverter::encoderInit(local.ThemeColor1),
                .has_ThemeColor2 = true,
                .ThemeColor2 = ColorConverter::encoderInit(local.ThemeColor2),
                .has_ThemeColor3 = true,
                .ThemeColor3 = ColorConverter::encoderInit(local.ThemeColor3)
        };
    }

    static ProtoType decoderInit(LocalType& local) {
        return ProtoType{
                .has_ThemeColor0 = true,
                .ThemeColor0 = ColorConverter::decoderInit(local.ThemeColor0),
                .has_ThemeColor1 = true,
                .ThemeColor1 = ColorConverter::decoderInit(local.ThemeColor1),
                .has_ThemeColor2 = true,
                .ThemeColor2 = ColorConverter::decoderInit(local.ThemeColor2),
                .has_ThemeColor3 = true,
                .ThemeColor3 = ColorConverter::decoderInit(local.ThemeColor3)
        };
    }

    static bool decoderApply(const ProtoType& proto, LocalType& local) {
        ColorConverter::decoderApply(proto.ThemeColor0, local.ThemeColor0);
        ColorConverter::decoderApply(proto.ThemeColor1, local.ThemeColor1);
        ColorConverter::decoderApply(proto.ThemeColor2, local.ThemeColor2);
        ColorConverter::decoderApply(proto.ThemeColor3, local.ThemeColor3);
        return true;
    }
};

/**
 * Converter for History message using nanopb_cpp
 */
class HistoryConverter : public MessageConverter<
        HistoryConverter,
        History,
        PROTO_History,
        &PROTO_History_msg>
{
public:
    static ProtoType encoderInit(const LocalType& local) {
        return ProtoType{
                .bass = ArrayConverter<FloatConverter, std::vector<float>>::encoderCallbackInit(local.bass),
                .lowmid = ArrayConverter<FloatConverter, std::vector<float>>::encoderCallbackInit(local.lowmid),
                .highmid = ArrayConverter<FloatConverter, std::vector<float>>::encoderCallbackInit(local.highmid),
                .treble = ArrayConverter<FloatConverter, std::vector<float>>::encoderCallbackInit(local.treble)
        };
    }

    static ProtoType decoderInit(LocalType& local) {
        return ProtoType{
                .bass = ArrayConverter<FloatConverter, std::vector<float>>::decoderCallbackInit(local.bass),
                .lowmid = ArrayConverter<FloatConverter, std::vector<float>>::decoderCallbackInit(local.lowmid),
                .highmid = ArrayConverter<FloatConverter, std::vector<float>>::decoderCallbackInit(local.highmid),
                .treble = ArrayConverter<FloatConverter, std::vector<float>>::decoderCallbackInit(local.treble)
        };
    }

    static bool decoderApply(const ProtoType& proto, LocalType& local) {
        return true;  // Vectors populated by callbacks
    }
};

/**
 * Converter for DFT message using nanopb_cpp
 */
class DFTConverter : public MessageConverter<
        DFTConverter,
        DFT,
        PROTO_DFT,
        &PROTO_DFT_msg>
{
public:
    static ProtoType encoderInit(const LocalType& local) {
        return ProtoType{
                .mag = ArrayConverter<FloatConverter, std::vector<float>>::encoderCallbackInit(local.mag),
                .magEQ = ArrayConverter<FloatConverter, std::vector<float>>::encoderCallbackInit(local.magEQ),
                .magfilt = ArrayConverter<FloatConverter, std::vector<float>>::encoderCallbackInit(local.magfilt),
                .magPhase = ArrayConverter<FloatConverter, std::vector<float>>::encoderCallbackInit(local.magPhase)
        };
    }

    static ProtoType decoderInit(LocalType& local) {
        return ProtoType{
                .mag = ArrayConverter<FloatConverter, std::vector<float>>::decoderCallbackInit(local.mag),
                .magEQ = ArrayConverter<FloatConverter, std::vector<float>>::decoderCallbackInit(local.magEQ),
                .magfilt = ArrayConverter<FloatConverter, std::vector<float>>::decoderCallbackInit(local.magfilt),
                .magPhase = ArrayConverter<FloatConverter, std::vector<float>>::decoderCallbackInit(local.magPhase)
        };
    }

    static bool decoderApply(const ProtoType& proto, LocalType& local) {
        return true;  // Vectors populated by callbacks
    }
};

/**
 * Converter for FilteredAudiolink message using nanopb_cpp
 */
class FilteredAudiolinkConverter : public MessageConverter<
        FilteredAudiolinkConverter,
        FilteredAudiolink,
        PROTO_FilteredAudiolink,
        &PROTO_FilteredAudiolink_msg>
{
public:
    static ProtoType encoderInit(const LocalType& local) {
        return ProtoType{
                .bass = ArrayConverter<FloatConverter, std::vector<float>>::encoderCallbackInit(local.bass),
                .lowmid = ArrayConverter<FloatConverter, std::vector<float>>::encoderCallbackInit(local.lowmid),
                .highmid = ArrayConverter<FloatConverter, std::vector<float>>::encoderCallbackInit(local.highmid),
                .treble = ArrayConverter<FloatConverter, std::vector<float>>::encoderCallbackInit(local.treble)
        };
    }

    static ProtoType decoderInit(LocalType& local) {
        return ProtoType{
                .bass = ArrayConverter<FloatConverter, std::vector<float>>::decoderCallbackInit(local.bass),
                .lowmid = ArrayConverter<FloatConverter, std::vector<float>>::decoderCallbackInit(local.lowmid),
                .highmid = ArrayConverter<FloatConverter, std::vector<float>>::decoderCallbackInit(local.highmid),
                .treble = ArrayConverter<FloatConverter, std::vector<float>>::decoderCallbackInit(local.treble)
        };
    }

    static bool decoderApply(const ProtoType& proto, LocalType& local) {
        return true;  // Vectors populated by callbacks
    }
};

class WaveFormConverter : public MessageConverter<
        WaveFormConverter,
        WaveForm,
        PROTO_WaveForm,
        &PROTO_WaveForm_msg>
{
public:
    static ProtoType encoderInit(const LocalType& local) {
        return ProtoType{
                .wav1 = ArrayConverter<FloatConverter, std::vector<float>>::encoderCallbackInit(local.wav1),
                .wav2 = ArrayConverter<FloatConverter, std::vector<float>>::encoderCallbackInit(local.wav2),
                .wav3 = ArrayConverter<FloatConverter, std::vector<float>>::encoderCallbackInit(local.wav3),
                .wav1diff = ArrayConverter<FloatConverter, std::vector<float>>::encoderCallbackInit(local.wav1diff)
        };
    }

    static ProtoType decoderInit(LocalType& local) {
        return ProtoType{
                .wav1 = ArrayConverter<FloatConverter, std::vector<float>>::decoderCallbackInit(local.wav1),
                .wav2 = ArrayConverter<FloatConverter, std::vector<float>>::decoderCallbackInit(local.wav2),
                .wav3 = ArrayConverter<FloatConverter, std::vector<float>>::decoderCallbackInit(local.wav3),
                .wav1diff = ArrayConverter<FloatConverter, std::vector<float>>::decoderCallbackInit(local.wav1diff)
        };
    }

    static bool decoderApply(const ProtoType& proto, LocalType& local) {
        return true;  // Vectors populated by callbacks
    }
};

class MediaStateConverter : public MessageConverter<
        MediaStateConverter,
        MediaState,
        PROTO_MediaState,
        &PROTO_MediaState_msg>
{
public:
    static ProtoType encoderInit(const LocalType& local) {
        return ProtoType{
                .mediaVolume = local.mediaVolume,
                .mediaTime = local.mediaTime,
                .mediaPlayback = PlaybackStateConverter::encoderInit(local.mediaPlayback),
                .mediaLoop = LoopOrRandomConverter::encoderInit(local.mediaLoop)
        };
    }

    static ProtoType decoderInit(LocalType& local) {
        return ProtoType{
                .mediaVolume = local.mediaVolume,
                .mediaTime = local.mediaTime,
                .mediaPlayback = PlaybackStateConverter::encoderInit(local.mediaPlayback),
                .mediaLoop = LoopOrRandomConverter::encoderInit(local.mediaLoop)
        };
    }

    static bool decoderApply(const ProtoType& proto, LocalType& local) {
        local.mediaVolume = proto.mediaVolume;
        local.mediaTime = proto.mediaTime;
        return PlaybackStateConverter::decoderApply(proto.mediaPlayback, local.mediaPlayback) &&
               LoopOrRandomConverter::decoderApply(proto.mediaLoop, local.mediaLoop);
    }
};

class PlayerDataConverter : public MessageConverter<
        PlayerDataConverter,
        PlayerData,
        PROTO_PlayerData,
        &PROTO_PlayerData_msg>
{
public:
    static ProtoType encoderInit(const LocalType& local) {
        return ProtoType{
                .numberOfPlayers = local.numberOfPlayers,
                .isMaster = local.isMaster,
                .isOwner = local.isOwner
        };
    }

    static ProtoType decoderInit(LocalType& local) {
        return ProtoType{
                .numberOfPlayers = local.numberOfPlayers,
                .isMaster = local.isMaster,
                .isOwner = local.isOwner
        };
    }

    static bool decoderApply(const ProtoType& proto, LocalType& local) {
        local.numberOfPlayers = proto.numberOfPlayers;
        local.isMaster = proto.isMaster;
        local.isOwner = proto.isOwner;
        return true;
    }
};

class IntensityConverter : public MessageConverter<
        IntensityConverter,
        Intensity,
        PROTO_Intensity,
        &PROTO_Intensity_msg>
{
public:
    static ProtoType encoderInit(const LocalType& local) {
        return ProtoType{
                .RMSLeft = local.RMSLeft,
                .PeakLeft = local.PeakLeft,
                .RMSRight = local.RMSRight,
                .PeakRight = local.PeakRight
        };
    }

    static ProtoType decoderInit(LocalType& local) {
        return ProtoType{
                .RMSLeft = local.RMSLeft,
                .PeakLeft = local.PeakLeft,
                .RMSRight = local.RMSRight,
                .PeakRight = local.PeakRight
        };
    }

    static bool decoderApply(const ProtoType& proto, LocalType& local) {
        local.RMSLeft = proto.RMSLeft;
        local.PeakLeft = proto.PeakLeft;
        local.RMSRight = proto.RMSRight;
        local.PeakRight = proto.PeakRight;
        return true;
    }
};

class AutogainConverter : public MessageConverter<
        AutogainConverter,
        Autogain,
        PROTO_Autogain,
        &PROTO_Autogain_msg>
{
public:
    static ProtoType encoderInit(const LocalType& local) {
        return ProtoType{
                .asymmetricGain = local.asymmetricGain,
                .symmetricGain = local.symmetricGain
        };
    }

    static ProtoType decoderInit(LocalType& local) {
        return ProtoType{
                .asymmetricGain = local.asymmetricGain,
                .symmetricGain = local.symmetricGain
        };
    }

    static bool decoderApply(const ProtoType& proto, LocalType& local) {
        local.asymmetricGain = proto.asymmetricGain;
        local.symmetricGain = proto.symmetricGain;
        return true;
    }
};

class PositionConverter : public MessageConverter<
        PositionConverter,
        Position,
        PROTO_Position,
        &PROTO_Position_msg>
{
public:
    static ProtoType encoderInit(const LocalType& local) {
        return ProtoType{
                .lat = local.lat,
                .lon = local.lon
        };
    }

    static ProtoType decoderInit(LocalType& local) {
        return ProtoType{
                .lat = local.lat,
                .lon = local.lon
        };
    }

    static bool decoderApply(const ProtoType& proto, LocalType& local) {
        local.lat = proto.lat;
        local.lon = proto.lon;
        return true;
    }
};

class GeneralVUConverter : public MessageConverter<
        GeneralVUConverter,
        GeneralVU,
        PROTO_GeneralVU,
        &PROTO_GeneralVU_msg>
{
public:
    static ProtoType encoderInit(const LocalType& local) {
        return ProtoType{
                .versionMajor = local.versionMajor,
                .versionMinor = local.versionMinor,
                .systemFPS = local.systemFPS,
                .frameCount = local.frameCount,
                .msSinceInstanceStart = local.msSinceInstanceStart,
                .msSinceMidnightLocal = local.msSinceMidnightLocal,
                .msInNetworkTime = local.msInNetworkTime,
                .has_media_state = true,
                .media_state = MediaStateConverter::encoderInit(local.media_state),
                .has_player_data = true,
                .player_data = PlayerDataConverter::encoderInit(local.player_data),
                .has_current_intensity = true,
                .current_intensity = IntensityConverter::encoderInit(local.current_intensity),
                .has_marker_value = true,
                .marker_value = IntensityConverter::encoderInit(local.marker_value),
                .has_marker_times = true,
                .marker_times = IntensityConverter::encoderInit(local.marker_times),
                .has_autogain = true,
                .autogain = AutogainConverter::encoderInit(local.autogain),
                .UTCDaysSinceEpoch = local.UTCDaysSinceEpoch,
                .msSinceUTCDayStart = local.msSinceUTCDayStart,
                .has_position = true,
                .position = PositionConverter::encoderInit(local.position)
        };
    }

    static ProtoType decoderInit(LocalType& local) {
        return ProtoType{
                .versionMajor = local.versionMajor,
                .versionMinor = local.versionMinor,
                .systemFPS = local.systemFPS,
                .frameCount = local.frameCount,
                .msSinceInstanceStart = local.msSinceInstanceStart,
                .msSinceMidnightLocal = local.msSinceMidnightLocal,
                .msInNetworkTime = local.msInNetworkTime,
                .has_media_state = true,
                .media_state = MediaStateConverter::decoderInit(local.media_state),
                .has_player_data = true,
                .player_data = PlayerDataConverter::decoderInit(local.player_data),
                .has_current_intensity = true,
                .current_intensity = IntensityConverter::decoderInit(local.current_intensity),
                .has_marker_value = true,
                .marker_value = IntensityConverter::decoderInit(local.marker_value),
                .has_marker_times = true,
                .marker_times = IntensityConverter::decoderInit(local.marker_times),
                .has_autogain = true,
                .autogain = AutogainConverter::decoderInit(local.autogain),
                .UTCDaysSinceEpoch = local.UTCDaysSinceEpoch,
                .msSinceUTCDayStart = local.msSinceUTCDayStart,
                .has_position = true,
                .position = PositionConverter::decoderInit(local.position)
        };
    }

    static bool decoderApply(const ProtoType& proto, LocalType& local) {
        local.versionMajor = proto.versionMajor;
        local.versionMinor = proto.versionMinor;
        local.systemFPS = proto.systemFPS;
        local.frameCount = proto.frameCount;
        local.msSinceInstanceStart = proto.msSinceInstanceStart;
        local.msSinceMidnightLocal = proto.msSinceMidnightLocal;
        local.msInNetworkTime = proto.msInNetworkTime;
        local.UTCDaysSinceEpoch = proto.UTCDaysSinceEpoch;
        local.msSinceUTCDayStart = proto.msSinceUTCDayStart;

        return MediaStateConverter::decoderApply(proto.media_state, local.media_state) &&
               PlayerDataConverter::decoderApply(proto.player_data, local.player_data) &&
               IntensityConverter::decoderApply(proto.current_intensity, local.current_intensity) &&
               IntensityConverter::decoderApply(proto.marker_value, local.marker_value) &&
               IntensityConverter::decoderApply(proto.marker_times, local.marker_times) &&
             AutogainConverter::decoderApply(proto.autogain, local.autogain) &&
             PositionConverter::decoderApply(proto.position, local.position);
    }
};

class ColorChordConverter : public MessageConverter<
        ColorChordConverter,
        ColorChord,
        PROTO_ColorChord,
        &PROTO_ColorChord_msg>
{
public:
    static ProtoType encoderInit(const LocalType& local) {
        return ProtoType{
                .colors = ArrayConverter<ColorConverter, std::vector<Color>>::encoderCallbackInit(local.colors),
                .strip = ArrayConverter<ColorConverter, std::vector<Color>>::encoderCallbackInit(local.strip),
                .lights_internal = ArrayConverter<ColorConverter, std::vector<Color>>::encoderCallbackInit(local.lights_internal),
                .lights = ArrayConverter<ColorConverter, std::vector<Color>>::encoderCallbackInit(local.lights)
        };
    }

    static ProtoType decoderInit(LocalType& local) {
        return ProtoType{
                .colors = ArrayConverter<ColorConverter, std::vector<Color>>::decoderCallbackInit(local.colors),
                .strip = ArrayConverter<ColorConverter, std::vector<Color>>::decoderCallbackInit(local.strip),
                .lights_internal = ArrayConverter<ColorConverter, std::vector<Color>>::decoderCallbackInit(local.lights_internal),
                .lights = ArrayConverter<ColorConverter, std::vector<Color>>::decoderCallbackInit(local.lights)
        };
    }

    static bool decoderApply(const ProtoType& proto, LocalType& local) {
        return true;  // Vectors populated by callbacks
    }
};

class AutoCorrelatorConverter : public MessageConverter<
        AutoCorrelatorConverter,
        AutoCorrelator,
        PROTO_AutoCorrelator,
        &PROTO_AutoCorrelator_msg>
{
public:
    static ProtoType encoderInit(const LocalType& local) {
        return ProtoType{
                .autocorrelation = ArrayConverter<FloatConverter, std::vector<float>>::encoderCallbackInit(local.autocorrelation),
                .uncorrelated = ArrayConverter<FloatConverter, std::vector<float>>::encoderCallbackInit(local.uncorrelated)
        };
    }

    static ProtoType decoderInit(LocalType& local) {
        return ProtoType{
                .autocorrelation = ArrayConverter<FloatConverter, std::vector<float>>::decoderCallbackInit(local.autocorrelation),
                .uncorrelated = ArrayConverter<FloatConverter, std::vector<float>>::decoderCallbackInit(local.uncorrelated)
        };
    }

    static bool decoderApply(const ProtoType& proto, LocalType& local) {
        return true;  // Vectors populated by callbacks
    }
};

class ChronotensityBandConverter : public MessageConverter<
        ChronotensityBandConverter,
        ChronotensityBand,
        PROTO_ChronotensityBand,
        &PROTO_ChronotensityBand_msg>
{
public:
    static ProtoType encoderInit(const LocalType& local) {
        return ProtoType{
                .increasing = local.increasing,
                .filtered_increasing = local.filtered_increasing,
                .bounce = local.bounce,
                .filtered_bounce = local.filtered_bounce,
                .intensity_pause = local.intensity_pause,
                .filtered_intensity_pause = local.filtered_intensity_pause,
                .bounce_pause = local.bounce_pause,
                .filtered_bounce_pause = local.filtered_bounce_pause
        };
    }

    static ProtoType decoderInit(LocalType& local) {
        return ProtoType{
                .increasing = local.increasing,
                .filtered_increasing = local.filtered_increasing,
                .bounce = local.bounce,
                .filtered_bounce = local.filtered_bounce,
                .intensity_pause = local.intensity_pause,
                .filtered_intensity_pause = local.filtered_intensity_pause,
                .bounce_pause = local.bounce_pause,
                .filtered_bounce_pause = local.filtered_bounce_pause
        };
    }

    static bool decoderApply(const ProtoType& proto, LocalType& local) {
        local.increasing = proto.increasing;
        local.filtered_increasing = proto.filtered_increasing;
        local.bounce = proto.bounce;
        local.filtered_bounce = proto.filtered_bounce;
        local.intensity_pause = proto.intensity_pause;
        local.filtered_intensity_pause = proto.filtered_intensity_pause;
        local.bounce_pause = proto.bounce_pause;
        local.filtered_bounce_pause = proto.filtered_bounce_pause;
        return true;
    }
};

class ChronotensityConverter : public MessageConverter<
        ChronotensityConverter,
        Chronotensity,
        PROTO_Chronotensity,
        &PROTO_Chronotensity_msg>
{
public:
    static ProtoType encoderInit(const LocalType& local) {
        return ProtoType{
                .has_bass = true,
                .bass = ChronotensityBandConverter::encoderInit(local.bass),
                .has_lowmid = true,
                .lowmid = ChronotensityBandConverter::encoderInit(local.lowmid),
                .has_highmid = true,
                .highmid = ChronotensityBandConverter::encoderInit(local.highmid),
                .has_treble = true,
                .treble = ChronotensityBandConverter::encoderInit(local.treble)
        };
    }

    static ProtoType decoderInit(LocalType& local) {
        return ProtoType{
                .has_bass = true,
                .bass = ChronotensityBandConverter::decoderInit(local.bass),
                .has_lowmid = true,
                .lowmid = ChronotensityBandConverter::decoderInit(local.lowmid),
                .has_highmid = true,
                .highmid = ChronotensityBandConverter::decoderInit(local.highmid),
                .has_treble = true,
                .treble = ChronotensityBandConverter::decoderInit(local.treble)
        };
    }

    static bool decoderApply(const ProtoType& proto, LocalType& local) {
        return ChronotensityBandConverter::decoderApply(proto.bass, local.bass) &&
               ChronotensityBandConverter::decoderApply(proto.lowmid, local.lowmid) &&
               ChronotensityBandConverter::decoderApply(proto.highmid, local.highmid) &&
               ChronotensityBandConverter::decoderApply(proto.treble, local.treble);
    }
};

class GlobalStringsConverter : public MessageConverter<
        GlobalStringsConverter,
        GlobalStrings,
        PROTO_GlobalStrings,
        &PROTO_GlobalStrings_msg>
{
public:
    static ProtoType encoderInit(const LocalType& local) {
        return ProtoType{
                .playerName = StringConverter::encoderInit(local.playerName),
                .masterName = StringConverter::encoderInit(local.masterName),
                .customString1 = StringConverter::encoderInit(local.customString1),
                .customString2 = StringConverter::encoderInit(local.customString2)
        };
    }

    static ProtoType decoderInit(LocalType& local) {
        return ProtoType{
                .playerName = StringConverter::decoderInit(local.playerName),
                .masterName = StringConverter::decoderInit(local.masterName),
                .customString1 = StringConverter::decoderInit(local.customString1),
                .customString2 = StringConverter::decoderInit(local.customString2)
        };
    }

    static bool decoderApply(const ProtoType& proto, LocalType& local) {
        return true;  // Strings populated by callbacks
    }
};

/**
 * Converter for Audiolink_Data message using nanopb_cpp
 */
class AudiolinkDataConverter : public MessageConverter<
        AudiolinkDataConverter,
        AudiolinkData,
        PROTO_Audiolink_Data,
        &PROTO_Audiolink_Data_msg>
{
public:
    static ProtoType encoderInit(const LocalType& local) {
        ProtoType proto = {};
        proto.has_history = true;
        proto.history = HistoryConverter::encoderInit(local.history);
        proto.has_theme_colors = true;
        proto.theme_colors = ThemeColorsConverter::encoderInit(local.theme_colors);
        proto.has_dft = true;
        proto.dft = DFTConverter::encoderInit(local.dft);
        proto.has_filtered_audiolink = true;
        proto.filtered_audiolink = FilteredAudiolinkConverter::encoderInit(local.filtered_audiolink);
        proto.has_waveform = true;
        proto.waveform = WaveFormConverter::encoderInit(local.waveform);
        proto.has_general_vu = true;
        proto.general_vu = GeneralVUConverter::encoderInit(local.general_vu);
        proto.has_colorchord = true;
        proto.colorchord = ColorChordConverter::encoderInit(local.colorchord);
        proto.has_autocorrelator = true;
        proto.autocorrelator = AutoCorrelatorConverter::encoderInit(local.autocorrelator);
        proto.has_chronotensity = true;
        proto.chronotensity = ChronotensityConverter::encoderInit(local.chronotensity);
        proto.has_global_strings = true;
        proto.global_strings = GlobalStringsConverter::encoderInit(local.global_strings);
        return proto;
    }

    static ProtoType decoderInit(LocalType& local) {
        ProtoType proto = {};
        proto.has_history = true;
        proto.history = HistoryConverter::decoderInit(local.history);
        proto.has_theme_colors = true;
        proto.theme_colors = ThemeColorsConverter::decoderInit(local.theme_colors);
        proto.has_dft = true;
        proto.dft = DFTConverter::decoderInit(local.dft);
        proto.has_filtered_audiolink = true;
        proto.filtered_audiolink = FilteredAudiolinkConverter::decoderInit(local.filtered_audiolink);
        proto.has_waveform = true;
        proto.waveform = WaveFormConverter::decoderInit(local.waveform);
        proto.has_general_vu = true;
        proto.general_vu = GeneralVUConverter::decoderInit(local.general_vu);
        proto.has_colorchord = true;
        proto.colorchord = ColorChordConverter::decoderInit(local.colorchord);
        proto.has_autocorrelator = true;
        proto.autocorrelator = AutoCorrelatorConverter::decoderInit(local.autocorrelator);
        proto.has_chronotensity = true;
        proto.chronotensity = ChronotensityConverter::decoderInit(local.chronotensity);
        proto.has_global_strings = true;
        proto.global_strings = GlobalStringsConverter::decoderInit(local.global_strings);
        return proto;
    }

    static bool decoderApply(const ProtoType& proto, LocalType& local) {
        return HistoryConverter::decoderApply(proto.history, local.history) &&
               ThemeColorsConverter::decoderApply(proto.theme_colors, local.theme_colors) &&
               DFTConverter::decoderApply(proto.dft, local.dft) &&
               FilteredAudiolinkConverter::decoderApply(proto.filtered_audiolink, local.filtered_audiolink) &&
               WaveFormConverter::decoderApply(proto.waveform, local.waveform) &&
               GeneralVUConverter::decoderApply(proto.general_vu, local.general_vu) &&
               ColorChordConverter::decoderApply(proto.colorchord, local.colorchord) &&
               AutoCorrelatorConverter::decoderApply(proto.autocorrelator, local.autocorrelator) &&
               ChronotensityConverter::decoderApply(proto.chronotensity, local.chronotensity) &&
               GlobalStringsConverter::decoderApply(proto.global_strings, local.global_strings);
    }
};


