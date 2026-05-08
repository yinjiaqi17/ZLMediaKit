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
    try {
        auto ret = ps_demuxer_input(static_cast<struct ps_demuxer_t *>(_ps_demuxer),
                                    reinterpret_cast<const uint8_t *>(ps->data()),
                                    ps->size());
        if (ret < 0) {
            _frames.clear();
        }
    } catch (toolkit::AssertFailedException &ex) {
        WarnL << "JT1078 step5 ps_demux_exception"
              << ", bytes: " << ps->size()
              << ", exception: " << ex.what()
              << ", hex: " << hexdump(ps->data(), std::min<size_t>(ps->size(), 32));
        _frames.clear();
    } catch (std::exception &ex) {
        WarnL << "JT1078 step5 ps_demux_exception"
              << ", bytes: " << ps->size()
              << ", exception: " << ex.what();
        _frames.clear();
    }
    return _frames;
}

void JT1078PsDemuxer::reset() {
    _frames.clear();
}

} // namespace mediakit
