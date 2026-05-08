/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include "JT1078PacketSplitter.h"

#include <algorithm>
#include <cstring>

using namespace toolkit;

namespace mediakit {

static constexpr size_t kV4HeaderSize = 32;

void JT1078PacketSplitter::input(const char *data, size_t len) {
    if (!data || !len) {
        return;
    }
    if (_cache.size() + len > _max_cache_size) {
        emitEvent("cache_overflow", _cache.size(), "jt1078 packet splitter cache overflow");
        _cache.clear();
    }
    _cache.append(data, len);

    while (_cache.size() >= kV4HeaderSize) {
        if (!isV4PacketHeader(_cache.data(), _cache.size(), 0)) {
            auto next = findV4PacketHeader(_cache.data(), _cache.size(), 1);
            auto consumed = next >= 0 ? (size_t)next : 1;
            emitEvent("invalid", consumed, "discard bytes before jt1078 v4 header");
            _cache.erase(0, consumed);
            continue;
        }

        auto next = findV4PacketHeader(_cache.data(), _cache.size(), 1);
        if (next < 0) {
            emitEvent("need_more", 0, "need next jt1078 v4 header to split tcp stream");
            return;
        }

        JT1078RtpPacket packet;
        size_t consumed = (size_t)next;
        std::string err;
        auto result = _decoder.input(_cache.data(), consumed, packet, consumed, err);
        switch (result) {
            case JT1078RtpDecoder::DecodeResult::Parsed: {
                if (_on_packet) {
                    _on_packet(packet, consumed);
                }
                _cache.erase(0, consumed);
                break;
            }
            case JT1078RtpDecoder::DecodeResult::NeedMore: {
                emitEvent("need_more", consumed, err);
                return;
            }
            case JT1078RtpDecoder::DecodeResult::Invalid: {
                if (!consumed) {
                    consumed = 1;
                }
                emitEvent("invalid", consumed, err);
                _cache.erase(0, std::min(consumed, _cache.size()));
                break;
            }
        }
    }

    if (_cache.size()) {
        emitEvent("need_more", 0, "need more bytes for jt1078 v4 header");
    }
}

void JT1078PacketSplitter::flush() {
    if (!_cache.size()) {
        return;
    }
    if (!isV4PacketHeader(_cache.data(), _cache.size(), 0)) {
        emitEvent("invalid", _cache.size(), "flush discard bytes without jt1078 v4 header");
        _cache.clear();
        return;
    }

    JT1078RtpPacket packet;
    size_t consumed = _cache.size();
    std::string err;
    auto result = _decoder.input(_cache.data(), _cache.size(), packet, consumed, err);
    if (result == JT1078RtpDecoder::DecodeResult::Parsed) {
        if (_on_packet) {
            _on_packet(packet, consumed);
        }
    } else {
        emitEvent(result == JT1078RtpDecoder::DecodeResult::NeedMore ? "need_more" : "invalid", consumed, err);
    }
    _cache.clear();
}

void JT1078PacketSplitter::reset() {
    _cache.clear();
}

size_t JT1078PacketSplitter::cachedSize() const {
    return _cache.size();
}

void JT1078PacketSplitter::setMaxCacheSize(size_t max_cache_size) {
    _max_cache_size = max_cache_size;
}

void JT1078PacketSplitter::setOnPacket(onPacket cb) {
    _on_packet = std::move(cb);
}

void JT1078PacketSplitter::setOnEvent(onEvent cb) {
    _on_event = std::move(cb);
}

bool JT1078PacketSplitter::isV4PacketHeader(const char *data, size_t len, size_t offset) const {
    if (!data || offset + kV4HeaderSize > len) {
        return false;
    }
    auto ptr = reinterpret_cast<const uint8_t *>(data + offset);
    if (((ptr[0] >> 6) & 0x03) != 2) {
        return false;
    }
    if (!isBcdSim(ptr + 12)) {
        return false;
    }
    auto channel = ptr[18];
    if (!channel) {
        return false;
    }
    auto data_type = (ptr[19] >> 4) & 0x0F;
    auto packet_type = ptr[19] & 0x0F;
    return data_type <= 0x04 && packet_type <= 0x03;
}

ssize_t JT1078PacketSplitter::findV4PacketHeader(const char *data, size_t len, size_t offset) const {
    if (!data || offset >= len) {
        return -1;
    }
    if (isV4PacketHeader(data, len, 0)) {
        auto ptr = reinterpret_cast<const uint8_t *>(data);
        auto payload_type = ptr[1] & 0x7F;
        for (size_t i = offset; i + kV4HeaderSize <= len; ++i) {
            auto current = reinterpret_cast<const uint8_t *>(data + i);
            if (((current[0] >> 6) & 0x03) != 2) {
                continue;
            }
            if ((current[1] & 0x7F) != payload_type) {
                continue;
            }
            if (std::memcmp(current + 12, ptr + 12, 6) != 0) {
                continue;
            }
            auto data_type = (current[19] >> 4) & 0x0F;
            auto packet_type = current[19] & 0x0F;
            if (current[18] && data_type <= 0x04 && packet_type <= 0x03) {
                return (ssize_t)i;
            }
        }
    }
    for (size_t i = offset; i + kV4HeaderSize <= len; ++i) {
        if (isV4PacketHeader(data, len, i)) {
            return (ssize_t)i;
        }
    }
    return -1;
}

bool JT1078PacketSplitter::isBcdSim(const uint8_t *data) const {
    for (size_t i = 0; i < 6; ++i) {
        if (((data[i] >> 4) & 0x0F) > 9 || (data[i] & 0x0F) > 9) {
            return false;
        }
    }
    return true;
}

void JT1078PacketSplitter::emitEvent(const char *stage, size_t consumed, const std::string &err) {
    if (_on_event) {
        _on_event(stage, consumed, err);
    }
}

} // namespace mediakit
