/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef ZLMEDIAKIT_JT1078PACKETSPLITTER_H
#define ZLMEDIAKIT_JT1078PACKETSPLITTER_H

#include "JT1078RtpDecoder.h"
#include "Network/Buffer.h"

#include <functional>
#include <memory>
#include <string>

namespace mediakit {

class JT1078PacketSplitter {
public:
    using Ptr = std::shared_ptr<JT1078PacketSplitter>;
    using onPacket = std::function<void(const JT1078RtpPacket &packet, size_t consumed)>;
    using onEvent = std::function<void(const char *stage, size_t consumed, const std::string &err)>;

    void input(const char *data, size_t len);
    void flush();
    void reset();

    size_t cachedSize() const;
    void setMaxCacheSize(size_t max_cache_size);
    void setOnPacket(onPacket cb);
    void setOnEvent(onEvent cb);

private:
    bool tryInputLengthPrefixedPacket(bool &matched);
    bool tryInputScannedPacket();
    bool inputPacket(const char *data, size_t len, size_t consumed, const char *stage);
    uint16_t loadBE16(const char *data) const;
    bool isV4PacketHeader(const char *data, size_t len, size_t offset) const;
    ssize_t findV4PacketHeader(const char *data, size_t len, size_t offset) const;
    bool isBcdSim(const uint8_t *data) const;
    void emitEvent(const char *stage, size_t consumed, const std::string &err);

private:
    size_t _max_cache_size = 1024 * 1024;
    toolkit::BufferLikeString _cache;
    JT1078RtpDecoder _decoder;
    onPacket _on_packet;
    onEvent _on_event;
};

} // namespace mediakit

#endif // ZLMEDIAKIT_JT1078PACKETSPLITTER_H
