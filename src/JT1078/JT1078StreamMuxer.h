/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
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

    bool start(const std::string &stream_id);
    bool inputFrame(const JT1078PsDemuxer::Frame &frame);
    void reset();

    bool started() const;
    const std::string &streamId() const;

private:
    bool addTrackIfNeed(const JT1078PsDemuxer::Frame &frame);

private:
    std::string _stream_id;
    MultiMediaSourceMuxer::Ptr _muxer;
    std::unordered_set<int> _track_added;
    bool _track_completed = false;
};

} // namespace mediakit

#endif // ZLMEDIAKIT_JT1078STREAMMUXER_H
