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
#include "JT1078PacketSplitter.h"
#include "JT1078PsDemuxer.h"
#include "JT1078RtpDecoder.h"
#include "JT1078SessionManager.h"
#include "JT1078StreamMuxer.h"
#include "JT1078TaskManager.h"
#include "Common/config.h"

using namespace toolkit;

namespace mediakit {

JT1078Session::JT1078Session(const Socket::Ptr &sock) : Session(sock) {
    GET_CONFIG(bool, wait_i_frame, "jt1078.waitIFrame");
    _packet_splitter = std::make_shared<JT1078PacketSplitter>();
    _packet_splitter->setOnPacket([this](const JT1078RtpPacket &packet, size_t consumed) {
        onRtpPacket(packet, consumed);
    });
    _packet_splitter->setOnEvent([this](const char *stage, size_t consumed, const std::string &err) {
        onSplitterEvent(stage, consumed, err);
    });
    _frame_assembler = std::make_shared<JT1078FrameAssembler>();
    _frame_assembler->setWaitIFrame(wait_i_frame);
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

void JT1078Session::closeByBiz(const std::string &reason) {
    WarnP(this) << "JT1078 session close by biz"
                << ", reason: " << (reason.empty() ? "-" : reason)
                << ", app: " << (_context.app.empty() ? "-" : _context.app)
                << ", stream_id: " << (_context.stream_id.empty() ? "-" : _context.stream_id)
                << ", sim: " << (_context.sim.empty() ? "-" : _context.sim)
                << ", channel: " << _context.channel
                << ", task_id: " << (_context.task_id.empty() ? "-" : _context.task_id);
    shutdown(SockException(Err_shutdown, reason.empty() ? "JT1078 session closed by biz" : reason));
}

const std::string &JT1078Session::app() const {
    return _context.app;
}

const std::string &JT1078Session::streamId() const {
    return _context.stream_id;
}

const std::string &JT1078Session::sim() const {
    return _context.sim;
}

int JT1078Session::channel() const {
    return _context.channel;
}

void JT1078Session::onInputData(const char *data, size_t len) {
    if (!data || !len) {
        logStep2State("empty_input", len);
        return;
    }

    InfoP(this) << "JT1078 recv tcp data from " << get_peer_ip() << ":" << get_peer_port()
                << ", len: " << len
                << ", cached: " << (_packet_splitter ? _packet_splitter->cachedSize() : 0)
                << ", total: " << _total_bytes;

    if (_packet_splitter) {
        _packet_splitter->input(data, len);
    }
    logStep2State("tcp_data_splitter_input", len);
}

void JT1078Session::onClose(const SockException &err) {
    if (_packet_splitter) {
        _packet_splitter->flush();
    }
    logStep2State("session_closing");
    WarnP(this) << "JT1078 client disconnected: " << get_peer_ip() << ":" << get_peer_port()
                << ", " << err
                << ", total bytes: " << _total_bytes
                << ", duration(s): " << _ticker.createdTime() / 1000;
    resetStreamContext();
}

void JT1078Session::resetStreamContext() {
    if (_context.registered) {
        JT1078SessionManager::Instance().removeSession(_context.app, _context.stream_id, _context.sim, _context.channel);
        _context.registered = false;
    }
    _context = JT1078SessionContext();
    if (_packet_splitter) {
        _packet_splitter->reset();
    }
    _packet_splitter.reset();
    _frame_assembler.reset();
    _ps_demuxer.reset();
    _stream_muxer.reset();
    logStep2State("stream_context_reset");
}

void JT1078Session::logStep2State(const char *stage, size_t incoming) {
    InfoP(this) << "JT1078 step2 " << stage
                << ", peer: " << get_peer_ip() << ":" << get_peer_port()
                << ", biz_type: " << JT1078TaskManager::bizTypeToString(_context.biz_type)
                << ", app: " << (_context.app.empty() ? "-" : _context.app)
                << ", sim: " << (_context.sim.empty() ? "-" : _context.sim)
                << ", channel: " << _context.channel
                << ", stream_id: " << (_context.stream_id.empty() ? "-" : _context.stream_id)
                << ", task_id: " << (_context.task_id.empty() ? "-" : _context.task_id)
                << ", bound: " << _context.bound
                << ", incoming: " << incoming
                << ", cached: " << (_packet_splitter ? _packet_splitter->cachedSize() : 0)
                << ", total: " << _total_bytes
                << ", packet_splitter: " << (_packet_splitter ? "ready" : "null")
                << ", frame_assembler: " << (_frame_assembler ? "ready" : "null")
                << ", ps_demuxer: " << (_ps_demuxer ? "ready" : "null")
                << ", stream_muxer: " << (_stream_muxer ? "ready" : "null");
}

void JT1078Session::onSplitterEvent(const char *stage, size_t consumed, const std::string &err) {
    logStep3Packet(stage, nullptr, consumed, err);
}

void JT1078Session::onRtpPacket(const JT1078RtpPacket &packet, size_t consumed) {
    logStep3Packet("parsed", &packet, consumed, "");

    if (!_context.bound) {
        bindSession(packet.sim, packet.channel);
    }

    InfoP(this) << "JT1078 step3 rtp_ready"
                << ", biz_type: " << JT1078TaskManager::bizTypeToString(_context.biz_type)
                << ", app: " << (_context.app.empty() ? "-" : _context.app)
                << ", stream_id: " << (_context.stream_id.empty() ? "-" : _context.stream_id)
                << ", sim: " << (_context.sim.empty() ? "-" : _context.sim)
                << ", channel: " << _context.channel
                << ", data_type: " << (int)packet.data_type << "(" << JT1078RtpDecoder::dataTypeToString(packet.data_type) << ")"
                << ", packet_type: " << (int)packet.packet_type << "(" << JT1078RtpDecoder::packetTypeToString(packet.packet_type) << ")"
                << ", seq: " << packet.sequence
                << ", timestamp: " << packet.timestamp
                << ", payload_size: " << packet.payload_size;

    if (!_frame_assembler) {
        WarnP(this) << "JT1078 step4 assembler_null, stream_id: " << (_context.stream_id.empty() ? "-" : _context.stream_id);
        return;
    }
    auto assemble_result = _frame_assembler->input(packet);
    logStep4Frame(assemble_result.output ? "frame_ready" : (assemble_result.dropped ? "frame_dropped" : "fragment_cached"), assemble_result);
    if (!assemble_result.output || !assemble_result.frame) {
        return;
    }

    if (!_ps_demuxer) {
        WarnP(this) << "JT1078 step5 demuxer_null, stream_id: " << (_context.stream_id.empty() ? "-" : _context.stream_id)
                    << ", ps_size: " << assemble_result.frame->size();
        return;
    }
    auto frames = _ps_demuxer->input(assemble_result.frame);
    InfoP(this) << "JT1078 step5 ps_demuxed"
                << ", app: " << (_context.app.empty() ? "-" : _context.app)
                << ", stream_id: " << (_context.stream_id.empty() ? "-" : _context.stream_id)
                << ", ps_size: " << assemble_result.frame->size()
                << ", frame_count: " << frames.size();
    for (auto &frame : frames) {
        logStep5Frame(frame);
        if (!_stream_muxer || !_stream_muxer->start(_context.app, _context.stream_id)) {
            WarnP(this) << "JT1078 step6 muxer_unavailable"
                        << ", app: " << (_context.app.empty() ? "-" : _context.app)
                        << ", stream_id: " << (_context.stream_id.empty() ? "-" : _context.stream_id)
                        << ", codec: " << getCodecName(frame.codec_id)
                        << ", payload_size: " << (frame.payload ? frame.payload->size() : 0);
            continue;
        }
        _stream_muxer->inputFrame(frame, assemble_result.timestamp);
    }
}

void JT1078Session::bindSession(const std::string &sim, int channel) {
    _context.sim = sim;
    _context.channel = channel;

    JT1078Task task;
    if (JT1078TaskManager::Instance().findPendingTask(sim, channel, task)) {
        _context.biz_type = task.biz_type;
        _context.app = task.app;
        _context.stream_id = task.stream_id;
        _context.task_id = task.task_id;
        _context.bound = true;
        JT1078TaskManager::Instance().removePendingTask(sim, channel);

        InfoP(this) << "JT1078 session bound pending task"
                    << ", biz_type: " << JT1078TaskManager::bizTypeToString(_context.biz_type)
                    << ", sim: " << sim
                    << ", channel: " << channel
                    << ", app: " << _context.app
                    << ", stream_id: " << _context.stream_id
                    << ", task_id: " << (_context.task_id.empty() ? "-" : _context.task_id);
    } else {
        _context.biz_type = JT1078BizType::Live;
        _context.app = "live";
        _context.stream_id = sim + "_" + std::to_string(channel) + "_live";
        _context.task_id.clear();
        _context.bound = true;

        InfoP(this) << "JT1078 session bound default live"
                    << ", sim: " << sim
                    << ", channel: " << channel
                    << ", app: " << _context.app
                    << ", stream_id: " << _context.stream_id;
    }

    auto self = std::dynamic_pointer_cast<JT1078Session>(shared_from_this());
    if (self) {
        JT1078SessionManager::Instance().addSession(_context.app, _context.stream_id, _context.sim, _context.channel, self);
        _context.registered = true;
    }
}

void JT1078Session::logStep3Packet(const char *stage, const JT1078RtpPacket *packet, size_t consumed, const std::string &err) {
    auto printer = InfoP(this);
    printer << "JT1078 step3 " << stage
            << ", peer: " << get_peer_ip() << ":" << get_peer_port()
            << ", cached: " << (_packet_splitter ? _packet_splitter->cachedSize() : 0)
            << ", consumed: " << consumed;
    if (packet) {
        printer << ", version: " << (int)packet->version
                << ", marker: " << packet->marker
                << ", pt: " << (int)packet->payload_type
                << ", seq: " << packet->sequence
                << ", timestamp: " << packet->timestamp
                << ", ssrc: " << packet->ssrc
                << ", sim: " << packet->sim
                << ", channel: " << (int)packet->channel
                << ", data_type: " << (int)packet->data_type << "(" << JT1078RtpDecoder::dataTypeToString(packet->data_type) << ")"
                << ", packet_type: " << (int)packet->packet_type << "(" << JT1078RtpDecoder::packetTypeToString(packet->packet_type) << ")"
                << ", key_frame: " << packet->key_frame
                << ", header_size: " << packet->header_size
                << ", payload_size: " << packet->payload_size
                << ", packet_size: " << packet->packet_size;
    }
    if (!err.empty()) {
        printer << ", err: " << err;
    }
}

void JT1078Session::logStep4Frame(const char *stage, const JT1078FrameAssembler::Result &result) {
    InfoP(this) << "JT1078 step4 " << stage
                << ", app: " << (_context.app.empty() ? "-" : _context.app)
                << ", stream_id: " << (_context.stream_id.empty() ? "-" : _context.stream_id)
                << ", sim: " << (_context.sim.empty() ? "-" : _context.sim)
                << ", channel: " << _context.channel
                << ", seq: " << result.sequence
                << ", timestamp: " << result.timestamp
                << ", data_type: " << (int)result.data_type << "(" << JT1078RtpDecoder::dataTypeToString(result.data_type) << ")"
                << ", key_frame: " << result.key_frame
                << ", payload_size: " << result.payload_size
                << ", frame_size: " << result.frame_size
                << ", cached: " << (_frame_assembler ? _frame_assembler->cachedSize() : 0)
                << ", err: " << (result.err.empty() ? "-" : result.err);
}

void JT1078Session::logStep5Frame(const JT1078PsDemuxer::Frame &frame) {
    InfoP(this) << "JT1078 step5 frame"
                << ", app: " << (_context.app.empty() ? "-" : _context.app)
                << ", stream_id: " << (_context.stream_id.empty() ? "-" : _context.stream_id)
                << ", codec: " << getCodecName(frame.codec_id)
                << ", stream: " << frame.stream
                << ", flags: " << frame.flags
                << ", pts: " << frame.pts
                << ", dts: " << frame.dts
                << ", payload_size: " << (frame.payload ? frame.payload->size() : 0);
}

} // namespace mediakit
