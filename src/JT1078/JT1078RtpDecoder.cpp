/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include "JT1078RtpDecoder.h"

using namespace std;

namespace mediakit {

static constexpr size_t kV4HeaderSize = 32;

JT1078RtpDecoder::DecodeResult JT1078RtpDecoder::input(const char *data, size_t len, JT1078RtpPacket &packet, size_t &consumed, string &err) const {
    consumed = 0;
    err.clear();
    if (!data || !len) {
        return DecodeResult::NeedMore;
    }
    if (len < kV4HeaderSize) {
        err = "need more bytes for jt1078 v4 header";
        return DecodeResult::NeedMore;
    }

    auto ptr = reinterpret_cast<const uint8_t *>(data);
    auto version = (ptr[0] >> 6) & 0x03;
    if (version != 2) {
        err = "invalid rtp version";
        consumed = 1;
        return DecodeResult::Invalid;
    }

    auto data_type = (ptr[19] >> 4) & 0x0F;
    auto packet_type = ptr[19] & 0x0F;
    switch (data_type) {
        case 0x00:
        case 0x01:
        case 0x02:
        case 0x03:
        case 0x04:
            break;
        default:
            err = "unsupported jt1078 data type";
            consumed = 1;
            return DecodeResult::Invalid;
    }
    if (packet_type > 0x03) {
        err = "unsupported jt1078 packet type";
        consumed = 1;
        return DecodeResult::Invalid;
    }

    auto payload_size = len - kV4HeaderSize;
    if (payload_size == 0) {
        err = "empty jt1078 payload";
        consumed = 1;
        return DecodeResult::Invalid;
    }

    packet = JT1078RtpPacket();
    packet.version = version;
    packet.padding = (ptr[0] & 0x20) != 0;
    packet.extension = (ptr[0] & 0x10) != 0;
    packet.csrc_count = ptr[0] & 0x0F;
    packet.marker = (ptr[1] & 0x80) != 0;
    packet.payload_type = ptr[1] & 0x7F;
    packet.sequence = loadBE16(ptr + 2);
    packet.timestamp = loadBE64(ptr + 20);
    packet.ssrc = loadBE32(ptr + 8);
    packet.sim = bcdToString(ptr + 12, 6);
    packet.channel = ptr[18];
    packet.data_type = data_type;
    packet.packet_type = packet_type;
    packet.key_frame = data_type == 0x00;
    packet.payload = ptr + kV4HeaderSize;
    packet.payload_size = payload_size;
    packet.header_size = kV4HeaderSize;
    packet.packet_size = len;
    consumed = len;
    return DecodeResult::Parsed;
}

const char *JT1078RtpDecoder::dataTypeToString(uint8_t data_type) {
    switch (data_type) {
        case 0x00: return "video_i";
        case 0x01: return "video_p";
        case 0x02: return "video_b";
        case 0x03: return "audio";
        case 0x04: return "passthrough";
        default: return "unknown";
    }
}

const char *JT1078RtpDecoder::packetTypeToString(uint8_t packet_type) {
    switch (packet_type) {
        case 0x00: return "atomic";
        case 0x01: return "first";
        case 0x02: return "last";
        case 0x03: return "middle";
        default: return "unknown";
    }
}

string JT1078RtpDecoder::bcdToString(const uint8_t *data, size_t len) {
    string ret;
    ret.reserve(len * 2);
    for (size_t i = 0; i < len; ++i) {
        auto high = (data[i] >> 4) & 0x0F;
        auto low = data[i] & 0x0F;
        if (high <= 9) {
            ret.push_back(char('0' + high));
        }
        if (low <= 9) {
            ret.push_back(char('0' + low));
        }
    }
    return ret;
}

uint16_t JT1078RtpDecoder::loadBE16(const uint8_t *data) {
    return (uint16_t(data[0]) << 8) | data[1];
}

uint32_t JT1078RtpDecoder::loadBE32(const uint8_t *data) {
    return (uint32_t(data[0]) << 24) | (uint32_t(data[1]) << 16) | (uint32_t(data[2]) << 8) | data[3];
}

uint64_t JT1078RtpDecoder::loadBE64(const uint8_t *data) {
    uint64_t ret = 0;
    for (size_t i = 0; i < 8; ++i) {
        ret = (ret << 8) | data[i];
    }
    return ret;
}

} // namespace mediakit
