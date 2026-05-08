/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include "JT1078TcpServer.h"
#include "JT1078Session.h"

using namespace toolkit;

namespace mediakit {

JT1078TcpServer::JT1078TcpServer() {
    _tcp_server = std::make_shared<TcpServer>();
}

void JT1078TcpServer::start(uint16_t local_port, const std::string &local_ip) {
    _tcp_server->start<JT1078Session>(local_port, local_ip);
    InfoL << "JT1078 tcp server started on " << local_ip << ":" << _tcp_server->getPort();
}

uint16_t JT1078TcpServer::getPort() const {
    return _tcp_server ? _tcp_server->getPort() : 0;
}

} // namespace mediakit
