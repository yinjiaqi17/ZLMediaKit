/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include "JT1078FrameAssembler.h"

#include <utility>

using namespace toolkit;

namespace mediakit {

JT1078FrameAssembler::Result JT1078FrameAssembler::input(const JT1078RtpPacket &packet) {
    Result ret;
    ret.key_frame = packet.key_frame;
    ret.data_type = packet.data_type;
    ret.sequence = packet.sequence;
    ret.timestamp = packet.timestamp;
    ret.payload_size = packet.payload_size;

    if (!packet.payload || !packet.payload_size) {
        return makeDrop(packet, "empty rtp payload");
    }

    switch (packet.packet_type) {
        case 0x00: {
            _assembling = false;
            _cache.clear();
            BufferLikeString buffer;
            buffer.append(reinterpret_cast<const char *>(packet.payload), packet.payload_size);
            return makeOutput(packet, std::move(buffer));
        }
        case 0x01: {
            _assembling = true;
            _expected_seq = nextSeq(packet.sequence);
            _timestamp = packet.timestamp;
            _data_type = packet.data_type;
            _cache.clear();
            _cache.append(reinterpret_cast<const char *>(packet.payload), packet.payload_size);
            if (_cache.size() > _max_cache_size) {
                reset();
                return makeDrop(packet, "assembled frame cache overflow on first packet");
            }
            ret.err = "fragment first cached";
            return ret;
        }
        case 0x03:
        case 0x02: {
            if (!_assembling) {
                return makeDrop(packet, "fragment packet without first packet");
            }
            if (packet.sequence != _expected_seq) {
                reset();
                return makeDrop(packet, "rtp sequence discontinuity");
            }
            if (packet.timestamp != _timestamp || packet.data_type != _data_type) {
                reset();
                return makeDrop(packet, "rtp fragment context changed");
            }
            if (_cache.size() + packet.payload_size > _max_cache_size) {
                reset();
                return makeDrop(packet, "assembled frame cache overflow");
            }
            _cache.append(reinterpret_cast<const char *>(packet.payload), packet.payload_size);
            _expected_seq = nextSeq(packet.sequence);
            if (packet.packet_type == 0x02) {
                auto buffer = std::move(_cache);
                _cache.clear();
                _assembling = false;
                return makeOutput(packet, std::move(buffer));
            }
            ret.err = "fragment middle cached";
            ret.frame_size = _cache.size();
            return ret;
        }
        default:
            return makeDrop(packet, "unsupported rtp packet type");
    }
}

void JT1078FrameAssembler::reset() {
    _assembling = false;
    _got_i_frame = false;
    _expected_seq = 0;
    _timestamp = 0;
    _data_type = 0;
    _cache.clear();
}

void JT1078FrameAssembler::setMaxCacheSize(size_t max_cache_size) {
    _max_cache_size = max_cache_size;
}

void JT1078FrameAssembler::setWaitIFrame(bool wait) {
    _wait_i_frame = wait;
}

size_t JT1078FrameAssembler::cachedSize() const {
    return _cache.size();
}

uint16_t JT1078FrameAssembler::nextSeq(uint16_t seq) {
    return uint16_t(seq + 1);
}

JT1078FrameAssembler::Result JT1078FrameAssembler::makeOutput(const JT1078RtpPacket &packet, BufferLikeString &&buffer) {
    Result ret;
    ret.output = true;
    ret.key_frame = packet.key_frame;
    ret.data_type = packet.data_type;
    ret.sequence = packet.sequence;
    ret.timestamp = packet.timestamp;
    ret.payload_size = packet.payload_size;
    ret.frame_size = buffer.size();

    if (_wait_i_frame && !_got_i_frame && !packet.key_frame) {
        ret.output = false;
        ret.dropped = true;
        ret.err = "wait first i frame";
        return ret;
    }
    if (packet.key_frame) {
        _got_i_frame = true;
    }
    ret.frame = std::make_shared<BufferLikeString>(std::move(buffer));
    return ret;
}

JT1078FrameAssembler::Result JT1078FrameAssembler::makeDrop(const JT1078RtpPacket &packet, const std::string &err) {
    Result ret;
    ret.dropped = true;
    ret.key_frame = packet.key_frame;
    ret.data_type = packet.data_type;
    ret.sequence = packet.sequence;
    ret.timestamp = packet.timestamp;
    ret.payload_size = packet.payload_size;
    ret.frame_size = _cache.size();
    ret.err = err;
    return ret;
}

} // namespace mediakit
