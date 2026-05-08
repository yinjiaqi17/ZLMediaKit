/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include "JT1078PsDemuxer.h"
#include "mpeg-ps.h"
#include "Util/util.h"

#include <algorithm>
#include <utility>

using namespace toolkit;

namespace mediakit {

JT1078PsDemuxer::JT1078PsDemuxer() {
    _ps_demuxer = ps_demuxer_create([](void *param,
                                       int stream,
                                       int codecid,
                                       int flags,
                                       int64_t pts,
                                       int64_t dts,
                                       const void *data,
                                       size_t bytes) {
        auto thiz = static_cast<JT1078PsDemuxer *>(param);
        CodecId codec_id = getCodecByMpegId(codecid);
        if (codec_id != CodecH264 && codec_id != CodecG711A) {
            InfoL << "JT1078 step5 ps_frame_ignored"
                  << ", stream: " << stream
                  << ", mpeg_codec: " << codecid
                  << ", codec: " << getCodecName(codec_id)
                  << ", flags: " << flags
                  << ", pts: " << pts
                  << ", dts: " << dts
                  << ", bytes: " << bytes;
            return 0;
        }
        Frame frame;
        frame.codec_id = codec_id;
        frame.stream = stream;
        frame.flags = flags;
        frame.pts = pts;
        frame.dts = dts;
        frame.payload = std::make_shared<BufferString>(std::string(static_cast<const char *>(data), bytes));
        thiz->_frames.emplace_back(std::move(frame));
        ++thiz->_decode_count;
        InfoL << "JT1078 step5 ps_frame_decoded"
              << ", total_decoded: " << thiz->_decode_count
              << ", stream: " << stream
              << ", mpeg_codec: " << codecid
              << ", codec: " << getCodecName(codec_id)
              << ", flags: " << flags
              << ", pts: " << pts
              << ", dts: " << dts
              << ", bytes: " << bytes;
        return 0;
    }, this);
}

JT1078PsDemuxer::~JT1078PsDemuxer() {
    if (_ps_demuxer) {
        ps_demuxer_destroy(static_cast<struct ps_demuxer_t *>(_ps_demuxer));
        _ps_demuxer = nullptr;
    }
}

std::vector<JT1078PsDemuxer::Frame> JT1078PsDemuxer::input(const Buffer::Ptr &ps) {
    _frames.clear();
    if (!_ps_demuxer || !ps || !ps->size()) {
        return {};
    }
    ++_input_count;
    InfoL << "JT1078 step5 ps_input"
          << ", input_index: " << _input_count
          << ", bytes: " << ps->size()
          << ", hex: " << hexdump(ps->data(), std::min<size_t>(ps->size(), 16));
    try {
        size_t offset = 0;
        while (offset < ps->size()) {
            auto remain = ps->size() - offset;
            auto ret = ps_demuxer_input(static_cast<struct ps_demuxer_t *>(_ps_demuxer),
                                        reinterpret_cast<const uint8_t *>(ps->data() + offset),
                                        remain);
            InfoL << "JT1078 step5 ps_consume"
                  << ", input_index: " << _input_count
                  << ", offset: " << offset
                  << ", remain: " << remain
                  << ", consumed: " << ret
                  << ", frames: " << _frames.size();
            if (ret < 0 || ret > static_cast<decltype(ret)>(remain)) {
                WarnL << "JT1078 step5 ps_demux_failed"
                      << ", input_index: " << _input_count
                      << ", bytes: " << ps->size()
                      << ", offset: " << offset
                      << ", remain: " << remain
                      << ", ret: " << ret
                      << ", hex: " << hexdump(ps->data() + offset, std::min<size_t>(remain, 32));
                _frames.clear();
                break;
            }
            if (ret == 0) {
                WarnL << "JT1078 step5 ps_demux_no_progress"
                      << ", input_index: " << _input_count
                      << ", bytes: " << ps->size()
                      << ", offset: " << offset
                      << ", remain: " << remain
                      << ", frames: " << _frames.size()
                      << ", hex: " << hexdump(ps->data() + offset, std::min<size_t>(remain, 32));
                break;
            }
            offset += static_cast<size_t>(ret);
        }
        if (_frames.empty()) {
            InfoL << "JT1078 step5 ps_output_empty"
                  << ", input_index: " << _input_count
                  << ", bytes: " << ps->size();
        } else {
            InfoL << "JT1078 step5 ps_output"
                  << ", input_index: " << _input_count
                  << ", frame_count: " << _frames.size();
        }
    } catch (toolkit::AssertFailedException &ex) {
        WarnL << "JT1078 step5 ps_demux_exception"
              << ", input_index: " << _input_count
              << ", bytes: " << ps->size()
              << ", exception: " << ex.what()
              << ", hex: " << hexdump(ps->data(), std::min<size_t>(ps->size(), 32));
        _frames.clear();
    } catch (std::exception &ex) {
        WarnL << "JT1078 step5 ps_demux_exception"
              << ", input_index: " << _input_count
              << ", bytes: " << ps->size()
              << ", exception: " << ex.what();
        _frames.clear();
    } catch (...) {
        WarnL << "JT1078 step5 ps_demux_exception"
              << ", input_index: " << _input_count
              << ", bytes: " << ps->size()
              << ", exception: unknown";
        _frames.clear();
    }
    return _frames;
}

void JT1078PsDemuxer::reset() {
    _frames.clear();
    _input_count = 0;
    _decode_count = 0;
}

} // namespace mediakit
