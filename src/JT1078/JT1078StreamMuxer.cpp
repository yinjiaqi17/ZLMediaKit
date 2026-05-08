/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include "JT1078StreamMuxer.h"
#include "Common/macros.h"
#include "Extension/Factory.h"

using namespace toolkit;

namespace mediakit {

bool JT1078StreamMuxer::start(const std::string &stream_id) {
    if (stream_id.empty()) {
        WarnL << "JT1078 step6 start muxer failed, empty stream id";
        return false;
    }
    if (_muxer && _stream_id == stream_id) {
        return true;
    }

    reset();
    _stream_id = stream_id;
    ProtocolOption option;
    MediaTuple tuple{DEFAULT_VHOST, "jt1078", _stream_id, ""};
    _muxer = std::make_shared<MultiMediaSourceMuxer>(tuple, 0.0f, option);
    InfoL << "JT1078 step6 muxer_started"
          << ", app: " << tuple.app
          << ", stream_id: " << _stream_id
          << ", short_url: " << tuple.shortUrl();
    return true;
}

bool JT1078StreamMuxer::inputFrame(const JT1078PsDemuxer::Frame &frame) {
    if (!_muxer) {
        WarnL << "JT1078 step6 input_frame_failed, muxer not started"
              << ", codec: " << getCodecName(frame.codec_id)
              << ", payload_size: " << (frame.payload ? frame.payload->size() : 0);
        return false;
    }
    if (!frame.payload || !frame.payload->size() || frame.codec_id == CodecInvalid) {
        WarnL << "JT1078 step6 input_frame_failed, invalid frame"
              << ", stream_id: " << _stream_id
              << ", codec: " << getCodecName(frame.codec_id)
              << ", payload_size: " << (frame.payload ? frame.payload->size() : 0);
        return false;
    }
    if (!addTrackIfNeed(frame)) {
        return false;
    }

    auto dts = frame.dts > 0 ? uint64_t(frame.dts / 90) : 0;
    auto pts = frame.pts > 0 ? uint64_t(frame.pts / 90) : dts;
    auto zlm_frame = Factory::getFrameFromBuffer(frame.codec_id, frame.payload, dts, pts);
    if (!zlm_frame) {
        WarnL << "JT1078 step6 input_frame_failed, create zlm frame failed"
              << ", stream_id: " << _stream_id
              << ", codec: " << getCodecName(frame.codec_id);
        return false;
    }
    zlm_frame->setIndex(frame.stream);
    auto ret = _muxer->inputFrame(zlm_frame);
    InfoL << "JT1078 step6 frame_input"
          << ", stream_id: " << _stream_id
          << ", codec: " << getCodecName(frame.codec_id)
          << ", track: " << frame.stream
          << ", dts: " << dts
          << ", pts: " << pts
          << ", payload_size: " << frame.payload->size()
          << ", ret: " << ret;
    return ret;
}

void JT1078StreamMuxer::reset() {
    if (_muxer) {
        InfoL << "JT1078 step6 muxer_reset"
              << ", stream_id: " << (_stream_id.empty() ? "-" : _stream_id);
    }
    _muxer.reset();
    _stream_id.clear();
    _track_added.clear();
    _track_completed = false;
}

bool JT1078StreamMuxer::started() const {
    return static_cast<bool>(_muxer);
}

const std::string &JT1078StreamMuxer::streamId() const {
    return _stream_id;
}

bool JT1078StreamMuxer::addTrackIfNeed(const JT1078PsDemuxer::Frame &frame) {
    if (_track_added.emplace(frame.stream).second) {
        auto track = Factory::getTrackByCodecId(frame.codec_id);
        if (!track) {
            WarnL << "JT1078 step6 add_track_failed"
                  << ", stream_id: " << _stream_id
                  << ", codec: " << getCodecName(frame.codec_id)
                  << ", track: " << frame.stream;
            return false;
        }
        track->setIndex(frame.stream);
        auto ret = _muxer->addTrack(track);
        InfoL << "JT1078 step6 track_added"
              << ", stream_id: " << _stream_id
              << ", codec: " << getCodecName(frame.codec_id)
              << ", track: " << frame.stream
              << ", ret: " << ret;
    }
    if (!_track_completed) {
        _muxer->addTrackCompleted();
        _track_completed = true;
        InfoL << "JT1078 step6 track_completed"
              << ", stream_id: " << _stream_id
              << ", track_count: " << _track_added.size();
    }
    return true;
}

} // namespace mediakit
