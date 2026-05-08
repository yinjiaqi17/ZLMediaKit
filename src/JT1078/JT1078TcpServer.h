/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef ZLMEDIAKIT_JT1078TCPSERVER_H
#define ZLMEDIAKIT_JT1078TCPSERVER_H

#include "Network/TcpServer.h"

namespace mediakit {

class JT1078TcpServer {
public:
    using Ptr = std::shared_ptr<JT1078TcpServer>;

    JT1078TcpServer();
    void start(uint16_t local_port, const std::string &local_ip = "::");
    uint16_t getPort() const;

private:
    toolkit::TcpServer::Ptr _tcp_server;
};

} // namespace mediakit

#endif // ZLMEDIAKIT_JT1078TCPSERVER_H
