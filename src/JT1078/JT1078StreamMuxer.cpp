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
namespace {

struct H264NalSummary {
    int first_type = -1;
    size_t nal_count = 0;
    size_t sps = 0;
    size_t pps = 0;
    size_t idr = 0;
    size_t slice = 0;
    size_t sei = 0;
    size_t aud = 0;
};

struct H264NalPart {
    size_t offset = 0;
    size_t size = 0;
    size_t prefix_size = 0;
    int type = -1;
};

size_t findStartCode(const char *data, size_t size, size_t pos, size_t &prefix_size) {
    while (pos + 3 <= size) {
        if (pos + 4 <= size && data[pos] == 0x00 && data[pos + 1] == 0x00 && data[pos + 2] == 0x00 && data[pos + 3] == 0x01) {
            prefix_size = 4;
            return pos;
        }
        if (data[pos] == 0x00 && data[pos + 1] == 0x00 && data[pos + 2] == 0x01) {
            prefix_size = 3;
            return pos;
        }
        ++pos;
    }
    return size;
}

H264NalSummary getH264NalSummary(const toolkit::Buffer::Ptr &payload) {
    H264NalSummary summary;
    if (!payload || payload->size() == 0) {
        return summary;
    }

    auto data = payload->data();
    auto size = payload->size();
    size_t prefix_size = 0;
    auto start = findStartCode(data, size, 0, prefix_size);
    if (start == size) {
        summary.nal_count = 1;
        summary.first_type = static_cast<uint8_t>(data[0]) & 0x1F;
        switch (summary.first_type) {
            case 1: ++summary.slice; break;
            case 5: ++summary.idr; break;
            case 6: ++summary.sei; break;
            case 7: ++summary.sps; break;
            case 8: ++summary.pps; break;
            case 9: ++summary.aud; break;
            default: break;
        }
        return summary;
    }

    while (start < size) {
        auto nal_pos = start + prefix_size;
        if (nal_pos >= size) {
            break;
        }
        auto type = static_cast<uint8_t>(data[nal_pos]) & 0x1F;
        if (summary.first_type < 0) {
            summary.first_type = type;
        }
        ++summary.nal_count;
        switch (type) {
            case 1: ++summary.slice; break;
            case 5: ++summary.idr; break;
            case 6: ++summary.sei; break;
            case 7: ++summary.sps; break;
            case 8: ++summary.pps; break;
            case 9: ++summary.aud; break;
            default: break;
        }
        start = findStartCode(data, size, nal_pos + 1, prefix_size);
    }
    return summary;
}

template <typename FUNC>
size_t forEachAnnexBNal(const toolkit::Buffer::Ptr &payload, FUNC &&func) {
    if (!payload || payload->size() == 0) {
        return 0;
    }

    auto data = payload->data();
    auto size = payload->size();
    size_t prefix_size = 0;
    auto start = findStartCode(data, size, 0, prefix_size);
    if (start == size) {
        return 0;
    }

    size_t count = 0;
    while (start < size) {
        auto nal_pos = start + prefix_size;
        if (nal_pos >= size) {
            break;
        }
        size_t next_prefix = 0;
        auto next = findStartCode(data, size, nal_pos + 1, next_prefix);
        H264NalPart part;
        part.offset = start;
        part.size = (next == size ? size : next) - start;
        part.prefix_size = prefix_size;
        part.type = static_cast<uint8_t>(data[nal_pos]) & 0x1F;
        if (part.size > part.prefix_size) {
            func(part);
            ++count;
        }
        start = next;
        prefix_size = next_prefix;
    }
    return count;
}

} // namespace

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

    auto rtp_stamp = fallback_timestamp / 90;
    const auto prefer_rtp_stamp = frame.codec_id == CodecH264 || frame.codec_id == CodecH265;
    const char *dts_source = prefer_rtp_stamp ? "rtp" : "demux";
    const char *pts_source = prefer_rtp_stamp ? "rtp" : "demux";
    auto demux_pts = frame.pts > 0 ? uint64_t(frame.pts / 90) : 0;
    auto demux_dts = frame.dts > 0 ? uint64_t(frame.dts / 90) : demux_pts;
    if (!prefer_rtp_stamp && !frame.dts && demux_dts) {
        dts_source = "demux_pts";
    }
    auto dts_input = prefer_rtp_stamp ? rtp_stamp : demux_dts;
    auto pts_input = prefer_rtp_stamp ? rtp_stamp : (demux_pts ? demux_pts : demux_dts);
    auto dts = normalizeStamp(dts_input, fallback_timestamp, _last_dts, dts_source);
    auto pts = normalizeStamp(pts_input ? pts_input : dts, fallback_timestamp, _last_pts, pts_source);
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
    H264NalSummary h264_summary;
    if (frame.codec_id == CodecH264) {
        h264_summary = getH264NalSummary(frame.payload);
        InfoL << "JT1078 step6 h264_nal"
              << ", stream_id: " << _stream_id
              << ", first_nal: " << h264_summary.first_type
              << ", nal_count: " << h264_summary.nal_count
              << ", sps: " << h264_summary.sps
              << ", pps: " << h264_summary.pps
              << ", idr: " << h264_summary.idr
              << ", slice: " << h264_summary.slice
              << ", sei: " << h264_summary.sei
              << ", aud: " << h264_summary.aud
              << ", zlm_prefix: " << zlm_frame->prefixSize()
              << ", zlm_key: " << zlm_frame->keyFrame()
              << ", zlm_config: " << zlm_frame->configFrame()
              << ", zlm_decode_able: " << zlm_frame->decodeAble()
              << ", dts: " << dts
              << ", pts: " << pts
              << ", payload_size: " << frame.payload->size();
    }
    if (frame.codec_id == CodecH264 && h264_summary.nal_count > 1 && (h264_summary.sps || h264_summary.pps) && (h264_summary.idr || h264_summary.slice)) {
        bool any_ret = false;
        size_t split_count = 0;
        auto data = frame.payload->data();
        forEachAnnexBNal(frame.payload, [&](const H264NalPart &part) {
            auto sub_payload = std::make_shared<BufferString>(std::string(data + part.offset, part.size));
            auto sub_frame = Factory::getFrameFromBuffer(frame.codec_id, sub_payload, dts, pts);
            if (!sub_frame) {
                WarnL << "JT1078 step6 h264_split_failed"
                      << ", stream_id: " << _stream_id
                      << ", nal_type: " << part.type
                      << ", nal_size: " << part.size;
                return;
            }
            sub_frame->setIndex(track_index);
            auto ret = _muxer->inputFrame(sub_frame);
            any_ret = any_ret || ret;
            ++split_count;
            InfoL << "JT1078 step6 h264_split_input"
                  << ", stream_id: " << _stream_id
                  << ", index: " << split_count
                  << ", nal_type: " << part.type
                  << ", nal_size: " << part.size
                  << ", prefix: " << part.prefix_size
                  << ", zlm_prefix: " << sub_frame->prefixSize()
                  << ", zlm_key: " << sub_frame->keyFrame()
                  << ", zlm_config: " << sub_frame->configFrame()
                  << ", zlm_decode_able: " << sub_frame->decodeAble()
                  << ", dts: " << dts
                  << ", pts: " << pts
                  << ", ret: " << ret;
        });
        InfoL << "JT1078 step6 frame_input"
              << ", stream_id: " << _stream_id
              << ", codec: " << getCodecName(frame.codec_id)
              << ", ps_stream: " << frame.stream
              << ", track: " << track_index
              << ", raw_pts: " << frame.pts
              << ", raw_dts: " << frame.dts
              << ", demux_pts: " << demux_pts
              << ", demux_dts: " << demux_dts
              << ", rtp_stamp: " << rtp_stamp
              << ", dts: " << dts
              << ", pts: " << pts
              << ", dts_source: " << dts_source
              << ", pts_source: " << pts_source
              << ", payload_size: " << frame.payload->size()
              << ", split: 1"
              << ", split_count: " << split_count
              << ", ret: " << any_ret;
        return any_ret;
    }
    auto ret = _muxer->inputFrame(zlm_frame);
    InfoL << "JT1078 step6 frame_input"
          << ", stream_id: " << _stream_id
          << ", codec: " << getCodecName(frame.codec_id)
          << ", ps_stream: " << frame.stream
          << ", track: " << track_index
          << ", raw_pts: " << frame.pts
          << ", raw_dts: " << frame.dts
          << ", demux_pts: " << demux_pts
          << ", demux_dts: " << demux_dts
          << ", rtp_stamp: " << rtp_stamp
          << ", dts: " << dts
          << ", pts: " << pts
          << ", dts_source: " << dts_source
          << ", pts_source: " << pts_source
          << ", payload_size: " << frame.payload->size()
          << ", split: 0"
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
        auto rtp_stamp = fallback_timestamp / 90;
        if (rtp_stamp > last_stamp) {
            stamp = rtp_stamp;
            stamp_source = "rtp_monotonic";
        } else {
            stamp = last_stamp + 1;
            stamp_source = "monotonic";
        }
    }
    last_stamp = stamp;
    return stamp;
}

} // namespace mediakit
