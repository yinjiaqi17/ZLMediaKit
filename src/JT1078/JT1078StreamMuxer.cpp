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

bool JT1078StreamMuxer::inputFrame(const JT1078PsDemuxer::Frame &frame, uint64_t fallback_timestamp) {
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
    auto track_index = getTrackIndex(frame);

    const char *dts_source = "demux";
    const char *pts_source = "demux";
    auto demux_pts = frame.pts > 0 ? uint64_t(frame.pts / 90) : 0;
    auto demux_dts = frame.dts > 0 ? uint64_t(frame.dts / 90) : demux_pts;
    if (!frame.dts && demux_dts) {
        dts_source = "demux_pts";
    }
    auto dts = normalizeStamp(demux_dts, fallback_timestamp, _last_dts, dts_source);
    auto pts = normalizeStamp(demux_pts ? demux_pts : dts, fallback_timestamp, _last_pts, pts_source);
    if (pts < dts) {
        pts = dts;
        _last_pts = pts;
        pts_source = dts_source;
    }
    auto zlm_frame = Factory::getFrameFromBuffer(frame.codec_id, frame.payload, dts, pts);
    if (!zlm_frame) {
        WarnL << "JT1078 step6 input_frame_failed, create zlm frame failed"
              << ", stream_id: " << _stream_id
              << ", codec: " << getCodecName(frame.codec_id);
        return false;
    }
    zlm_frame->setIndex(track_index);
    auto ret = _muxer->inputFrame(zlm_frame);
    InfoL << "JT1078 step6 frame_input"
          << ", stream_id: " << _stream_id
          << ", codec: " << getCodecName(frame.codec_id)
          << ", ps_stream: " << frame.stream
          << ", track: " << track_index
          << ", dts: " << dts
          << ", pts: " << pts
          << ", dts_source: " << dts_source
          << ", pts_source: " << pts_source
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
    _last_dts = 0;
    _last_pts = 0;
}

bool JT1078StreamMuxer::started() const {
    return static_cast<bool>(_muxer);
}

const std::string &JT1078StreamMuxer::streamId() const {
    return _stream_id;
}

bool JT1078StreamMuxer::addTrackIfNeed(const JT1078PsDemuxer::Frame &frame) {
    auto track_index = getTrackIndex(frame);
    if (_track_added.emplace(track_index).second) {
        auto track = Factory::getTrackByCodecId(frame.codec_id);
        if (!track) {
            WarnL << "JT1078 step6 add_track_failed"
                  << ", stream_id: " << _stream_id
                  << ", codec: " << getCodecName(frame.codec_id)
                  << ", ps_stream: " << frame.stream
                  << ", track: " << track_index;
            return false;
        }
        track->setIndex(track_index);
        auto ret = _muxer->addTrack(track);
        InfoL << "JT1078 step6 track_added"
              << ", stream_id: " << _stream_id
              << ", codec: " << getCodecName(frame.codec_id)
              << ", ps_stream: " << frame.stream
              << ", track: " << track_index
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

int JT1078StreamMuxer::getTrackIndex(const JT1078PsDemuxer::Frame &frame) const {
    auto track_type = getTrackType(frame.codec_id);
    if (track_type == TrackVideo || track_type == TrackAudio) {
        return track_type;
    }
    return frame.stream;
}

uint64_t JT1078StreamMuxer::normalizeStamp(uint64_t demux_stamp, uint64_t fallback_timestamp, uint64_t &last_stamp, const char *&stamp_source) {
    uint64_t stamp = demux_stamp;
    if (!stamp) {
        stamp = fallback_timestamp / 90;
        stamp_source = "rtp";
    }
    if (last_stamp && stamp < last_stamp) {
        stamp = last_stamp + 1;
        stamp_source = "monotonic";
    }
    last_stamp = stamp;
    return stamp;
}

} // namespace mediakit
