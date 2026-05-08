/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef ZLMEDIAKIT_JT1078FRAMEASSEMBLER_H
#define ZLMEDIAKIT_JT1078FRAMEASSEMBLER_H

#include "JT1078RtpDecoder.h"
#include "Network/Buffer.h"

#include <memory>
#include <string>

namespace mediakit {

class JT1078FrameAssembler {
public:
    using Ptr = std::shared_ptr<JT1078FrameAssembler>;

    struct Result {
        bool output = false;
        bool dropped = false;
        bool key_frame = false;
        uint8_t data_type = 0;
        uint16_t sequence = 0;
        uint64_t timestamp = 0;
        size_t payload_size = 0;
        size_t frame_size = 0;
        toolkit::Buffer::Ptr frame;
        std::string err;
    };

    Result input(const JT1078RtpPacket &packet);
    void reset();
    void setMaxCacheSize(size_t max_cache_size);
    void setWaitIFrame(bool wait);
    size_t cachedSize() const;

private:
    static uint16_t nextSeq(uint16_t seq);
    Result makeOutput(const JT1078RtpPacket &packet, toolkit::BufferLikeString &&buffer);
    Result makeDrop(const JT1078RtpPacket &packet, const std::string &err);

private:
    bool _assembling = false;
    bool _wait_i_frame = true;
    bool _got_i_frame = false;
    uint16_t _expected_seq = 0;
    uint64_t _timestamp = 0;
    uint8_t _data_type = 0;
    size_t _max_cache_size = 2 * 1024 * 1024;
    toolkit::BufferLikeString _cache;
};

} // namespace mediakit

#endif // ZLMEDIAKIT_JT1078FRAMEASSEMBLER_H
