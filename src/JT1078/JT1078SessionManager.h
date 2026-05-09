/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree.
 */

#ifndef ZLMEDIAKIT_JT1078SESSIONMANAGER_H
#define ZLMEDIAKIT_JT1078SESSIONMANAGER_H

#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

namespace mediakit {

class JT1078Session;

class JT1078SessionManager {
public:
    static JT1078SessionManager &Instance();

    void addSession(const std::string &app,
                    const std::string &stream_id,
                    const std::string &sim,
                    int channel,
                    const std::weak_ptr<JT1078Session> &session);
    void removeSession(const std::string &app,
                       const std::string &stream_id,
                       const std::string &sim,
                       int channel);
    std::shared_ptr<JT1078Session> findSession(const std::string &app, const std::string &stream_id);
    std::shared_ptr<JT1078Session> findSessionByDevice(const std::string &sim, int channel);

private:
    static std::string makeStreamKey(const std::string &app, const std::string &stream_id);
    static std::string makeDeviceKey(const std::string &sim, int channel);

private:
    std::mutex _mtx;
    std::unordered_map<std::string, std::weak_ptr<JT1078Session> > _sessions;
    std::unordered_map<std::string, std::string> _device_to_stream;
};

} // namespace mediakit

#endif // ZLMEDIAKIT_JT1078SESSIONMANAGER_H
