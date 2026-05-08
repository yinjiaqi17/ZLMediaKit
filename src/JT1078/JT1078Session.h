/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef ZLMEDIAKIT_JT1078SESSION_H
#define ZLMEDIAKIT_JT1078SESSION_H

#include "Network/Session.h"
#include "Network/Buffer.h"
#include "Util/TimeTicker.h"

namespace mediakit {

class JT1078RtpDecoder;
class JT1078FrameAssembler;
class JT1078PsDemuxer;
class JT1078StreamMuxer;

class JT1078Session : public toolkit::Session {
public:
    using Ptr = std::shared_ptr<JT1078Session>;

    JT1078Session(const toolkit::Socket::Ptr &sock);

    void onRecv(const toolkit::Buffer::Ptr &buf) override;
    void onError(const toolkit::SockException &err) override;
    void onManager() override;

private:
    void onInputData(const char *data, size_t len);
    void onClose(const toolkit::SockException &err);
    void resetStreamContext();
    void logStep2State(const char *stage, size_t incoming = 0);

private:
    std::string _sim;
    int _channel = 0;
    std::string _stream_id;

    uint64_t _total_bytes = 0;
    toolkit::BufferLikeString _recv_buffer;
    toolkit::Ticker _ticker;

    std::shared_ptr<JT1078RtpDecoder> _rtp_decoder;
    std::shared_ptr<JT1078FrameAssembler> _frame_assembler;
    std::shared_ptr<JT1078PsDemuxer> _ps_demuxer;
    std::shared_ptr<JT1078StreamMuxer> _stream_muxer;
};

} // namespace mediakit

#endif // ZLMEDIAKIT_JT1078SESSION_H
