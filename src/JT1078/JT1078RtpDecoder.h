/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef ZLMEDIAKIT_JT1078RTPDECODER_H
#define ZLMEDIAKIT_JT1078RTPDECODER_H

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>

namespace mediakit {

struct JT1078RtpPacket {
    uint8_t version = 0;
    bool padding = false;
    bool extension = false;
    uint8_t csrc_count = 0;
    bool marker = false;
    uint8_t payload_type = 0;
    uint16_t sequence = 0;
    uint64_t timestamp = 0;
    uint32_t ssrc = 0;

    std::string sim;
    uint8_t channel = 0;
    uint8_t data_type = 0;
    uint8_t packet_type = 0;
    bool key_frame = false;

    const uint8_t *payload = nullptr;
    size_t payload_size = 0;
    size_t header_size = 0;
    size_t packet_size = 0;
};

class JT1078RtpDecoder {
public:
    using Ptr = std::shared_ptr<JT1078RtpDecoder>;

    enum class DecodeResult {
        Parsed,
        NeedMore,
        Invalid
    };

    DecodeResult input(const char *data, size_t len, JT1078RtpPacket &packet, size_t &consumed, std::string &err) const;

    static const char *dataTypeToString(uint8_t data_type);
    static const char *packetTypeToString(uint8_t packet_type);

private:
    static std::string bcdToString(const uint8_t *data, size_t len);
    static uint16_t loadBE16(const uint8_t *data);
    static uint32_t loadBE32(const uint8_t *data);
    static uint64_t loadBE64(const uint8_t *data);
};

} // namespace mediakit

#endif // ZLMEDIAKIT_JT1078RTPDECODER_H
