/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef ZLMEDIAKIT_JT1078PSDEMUXER_H
#define ZLMEDIAKIT_JT1078PSDEMUXER_H

#include "Extension/Frame.h"
#include "Network/Buffer.h"

#include <memory>
#include <vector>

namespace mediakit {

class JT1078PsDemuxer {
public:
    using Ptr = std::shared_ptr<JT1078PsDemuxer>;

    struct Frame {
        CodecId codec_id = CodecInvalid;
        int stream = 0;
        int flags = 0;
        int64_t pts = 0;
        int64_t dts = 0;
        toolkit::Buffer::Ptr payload;
    };

    JT1078PsDemuxer();
    ~JT1078PsDemuxer();

    std::vector<Frame> input(const toolkit::Buffer::Ptr &ps);
    void reset();

private:
    void *_ps_demuxer = nullptr;
    std::vector<Frame> _frames;
};

} // namespace mediakit

#endif // ZLMEDIAKIT_JT1078PSDEMUXER_H
