/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include "JT1078Session.h"
#include "JT1078FrameAssembler.h"
#include "JT1078PsDemuxer.h"
#include "JT1078RtpDecoder.h"
#include "JT1078StreamMuxer.h"
#include "Common/config.h"

using namespace toolkit;

namespace mediakit {

static constexpr size_t kMaxRecvCacheSize = 1024 * 1024;

JT1078Session::JT1078Session(const Socket::Ptr &sock) : Session(sock) {
    _rtp_decoder = std::make_shared<JT1078RtpDecoder>();
    _frame_assembler = std::make_shared<JT1078FrameAssembler>();
    _ps_demuxer = std::make_shared<JT1078PsDemuxer>();
    _stream_muxer = std::make_shared<JT1078StreamMuxer>();
    InfoP(this) << "JT1078 client connected: " << get_peer_ip() << ":" << get_peer_port();
    logStep2State("session_created");
}

void JT1078Session::onRecv(const Buffer::Ptr &buf) {
    _ticker.resetTime();
    _total_bytes += buf->size();
    logStep2State("on_recv", buf->size());
    onInputData(buf->data(), buf->size());
}

void JT1078Session::onError(const SockException &err) {
    onClose(err);
}

void JT1078Session::onManager() {
    GET_CONFIG(uint32_t, timeout_sec, "jt1078.timeoutSec");
    if (_ticker.elapsedTime() > timeout_sec * 1000) {
        shutdown(SockException(Err_timeout, "JT1078 session timeout"));
    }
}

void JT1078Session::onInputData(const char *data, size_t len) {
    if (!data || !len) {
        logStep2State("empty_input", len);
        return;
    }
    if (_recv_buffer.size() + len > kMaxRecvCacheSize) {
        WarnP(this) << "JT1078 recv cache overflow, peer: " << get_peer_ip() << ":" << get_peer_port()
                    << ", cache: " << _recv_buffer.size()
                    << ", incoming: " << len;
        _recv_buffer.clear();
        logStep2State("recv_cache_cleared", len);
    }
    _recv_buffer.append(data, len);

    InfoP(this) << "JT1078 recv tcp data from " << get_peer_ip() << ":" << get_peer_port()
                << ", len: " << len
                << ", cached: " << _recv_buffer.size()
                << ", total: " << _total_bytes;
    logStep2State("tcp_data_cached", len);

    // Step 3 will parse JT1078 RTP packets from _recv_buffer here.
}

void JT1078Session::onClose(const SockException &err) {
    logStep2State("session_closing");
    WarnP(this) << "JT1078 client disconnected: " << get_peer_ip() << ":" << get_peer_port()
                << ", " << err
                << ", total bytes: " << _total_bytes
                << ", duration(s): " << _ticker.createdTime() / 1000;
    resetStreamContext();
}

void JT1078Session::resetStreamContext() {
    _sim.clear();
    _channel = 0;
    _stream_id.clear();
    _recv_buffer.clear();
    _rtp_decoder.reset();
    _frame_assembler.reset();
    _ps_demuxer.reset();
    _stream_muxer.reset();
    logStep2State("stream_context_reset");
}

void JT1078Session::logStep2State(const char *stage, size_t incoming) {
    InfoP(this) << "JT1078 step2 " << stage
                << ", peer: " << get_peer_ip() << ":" << get_peer_port()
                << ", sim: " << (_sim.empty() ? "-" : _sim)
                << ", channel: " << _channel
                << ", stream_id: " << (_stream_id.empty() ? "-" : _stream_id)
                << ", incoming: " << incoming
                << ", cached: " << _recv_buffer.size()
                << ", total: " << _total_bytes
                << ", rtp_decoder: " << (_rtp_decoder ? "ready" : "null")
                << ", frame_assembler: " << (_frame_assembler ? "ready" : "null")
                << ", ps_demuxer: " << (_ps_demuxer ? "ready" : "null")
                << ", stream_muxer: " << (_stream_muxer ? "ready" : "null");
}

} // namespace mediakit
