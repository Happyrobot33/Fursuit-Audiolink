#pragma once

#include <vector>
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
        return proto;
    }

    static bool decoderApply(const ProtoType& proto, LocalType& local) {
        return HistoryConverter::decoderApply(proto.history, local.history) &&
               ThemeColorsConverter::decoderApply(proto.theme_colors, local.theme_colors) &&
               DFTConverter::decoderApply(proto.dft, local.dft) &&
               FilteredAudiolinkConverter::decoderApply(proto.filtered_audiolink, local.filtered_audiolink);
    }
};


