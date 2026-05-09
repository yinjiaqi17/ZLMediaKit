/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree.
 */

#ifndef ZLMEDIAKIT_JT1078STREAMMUXER_H
#define ZLMEDIAKIT_JT1078STREAMMUXER_H

#include "JT1078PsDemuxer.h"
#include "Common/MultiMediaSourceMuxer.h"

#include <memory>
#include <string>
#include <unordered_set>

namespace mediakit {

class JT1078StreamMuxer {
public:
    using Ptr = std::shared_ptr<JT1078StreamMuxer>;

    bool start(const std::string &app, const std::string &stream_id);
    bool start(const std::string &stream_id);
    bool inputFrame(const JT1078PsDemuxer::Frame &frame, uint64_t fallback_timestamp);
    void reset();

    bool started() const;
    const std::string &app() const;
    const std::string &streamId() const;

private:
    bool addTrackIfNeed(const JT1078PsDemuxer::Frame &frame);
    int getTrackIndex(const JT1078PsDemuxer::Frame &frame) const;
    uint64_t normalizeStamp(uint64_t demux_stamp, uint64_t fallback_timestamp, uint64_t &last_stamp, const char *&stamp_source);

    // H264 Access Unit 处理：
    // JT1078/PS 中一个视频帧可能包含多个 NAL，尤其是多 slice IDR/P 帧。
    // 不能把每个 VCL NAL 都单独 inputFrame 给 ZLM，否则 FLV/flv.js 侧容易周期性重同步。
    bool flushH264AccessUnit(int track_index);
    void appendH264NalToAccessUnit(const char *data, size_t size, uint64_t dts, uint64_t pts, bool key);
    void clearH264AccessUnit();

private:
    std::string _app;
    std::string _stream_id;
    MultiMediaSourceMuxer::Ptr _muxer;
    std::unordered_set<int> _track_added;
    bool _track_completed = false;

    // 只第一次输入 SPS/PPS，后续 GOP 重复 SPS/PPS 跳过，避免浏览器端周期性“刷新圈”。
    bool _h264_sps_input = false;
    bool _h264_pps_input = false;

    // H264 Access Unit 缓存，同一 dts/pts 的多个 VCL NAL 合并后再输入 ZLM。
    std::string _h264_au_cache;
    uint64_t _h264_au_dts = 0;
    uint64_t _h264_au_pts = 0;
    bool _h264_au_started = false;
    bool _h264_au_key = false;
    size_t _h264_au_nal_count = 0;

    uint64_t _last_dts = 0;
    uint64_t _last_pts = 0;
};

} // namespace mediakit

#endif // ZLMEDIAKIT_JT1078STREAMMUXER_H
