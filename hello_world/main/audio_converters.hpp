#pragma once

#include <vector>
#include "audiolink_data.pb.h"
#include "nanopb_cpp.h"
#include "receiver.h"

using namespace NanoPb::Converter;

/**
 * Converter for History message using nanopb_cpp
 */
class HistoryConverter : public MessageConverter<
        HistoryConverter,
        CppHistory,
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
 * Converter for Audiolink_Data message using nanopb_cpp
 */
class AudiolinkDataConverter : public MessageConverter<
        AudiolinkDataConverter,
        CppAudiolinkData,
        PROTO_Audiolink_Data,
        &PROTO_Audiolink_Data_msg>
{
public:
    static ProtoType encoderInit(const LocalType& local) {
        ProtoType proto = {};
        proto.history = HistoryConverter::encoderInit(local.history);
        return proto;
    }

    static ProtoType decoderInit(LocalType& local) {
        ProtoType proto = {};
        proto.history = HistoryConverter::decoderInit(local.history);
        return proto;
    }

    static bool decoderApply(const ProtoType& proto, LocalType& local) {
        return HistoryConverter::decoderApply(proto.history, local.history);
    }
};
