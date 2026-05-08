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

using namespace toolkit;

namespace mediakit {

JT1078Session::JT1078Session(const Socket::Ptr &sock) : Session(sock) {
    InfoP(this) << "JT1078 client connected: " << get_peer_ip() << ":" << get_peer_port();
}

void JT1078Session::onRecv(const Buffer::Ptr &buf) {
    _ticker.resetTime();
    _total_bytes += buf->size();
    InfoP(this) << "JT1078 recv packet from " << get_peer_ip() << ":" << get_peer_port()
                << ", len: " << buf->size()
                << ", total: " << _total_bytes;
}

void JT1078Session::onError(const SockException &err) {
    WarnP(this) << "JT1078 client disconnected: " << get_peer_ip() << ":" << get_peer_port()
                << ", " << err
                << ", total bytes: " << _total_bytes
                << ", duration(s): " << _ticker.createdTime() / 1000;
}

void JT1078Session::onManager() {
    if (_ticker.elapsedTime() > 60 * 1000) {
        shutdown(SockException(Err_timeout, "JT1078 session timeout"));
    }
}

} // namespace mediakit
