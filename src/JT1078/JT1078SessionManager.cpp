/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree.
 */

#include "JT1078SessionManager.h"
#include "Util/logger.h"

namespace mediakit {

JT1078SessionManager &JT1078SessionManager::Instance() {
    static JT1078SessionManager instance;
    return instance;
}

void JT1078SessionManager::addSession(const std::string &app,
                                      const std::string &stream_id,
                                      const std::string &sim,
                                      int channel,
                                      const std::weak_ptr<JT1078Session> &session) {
    auto stream_key = makeStreamKey(app, stream_id);
    auto device_key = makeDeviceKey(sim, channel);

    std::lock_guard<std::mutex> lck(_mtx);
    _sessions[stream_key] = session;
    _device_to_stream[device_key] = stream_key;

    InfoL << "JT1078 session registered"
          << ", sim: " << sim
          << ", channel: " << channel
          << ", app: " << app
          << ", stream_id: " << stream_id;
}

void JT1078SessionManager::removeSession(const std::string &app,
                                         const std::string &stream_id,
                                         const std::string &sim,
                                         int channel) {
    auto stream_key = makeStreamKey(app, stream_id);
    auto device_key = makeDeviceKey(sim, channel);

    std::lock_guard<std::mutex> lck(_mtx);
    _sessions.erase(stream_key);
    auto it = _device_to_stream.find(device_key);
    if (it != _device_to_stream.end() && it->second == stream_key) {
        _device_to_stream.erase(it);
    }

    InfoL << "JT1078 session unregistered"
          << ", sim: " << (sim.empty() ? "-" : sim)
          << ", channel: " << channel
          << ", app: " << (app.empty() ? "-" : app)
          << ", stream_id: " << (stream_id.empty() ? "-" : stream_id);
}

std::shared_ptr<JT1078Session> JT1078SessionManager::findSession(const std::string &app, const std::string &stream_id) {
    std::lock_guard<std::mutex> lck(_mtx);
    auto it = _sessions.find(makeStreamKey(app, stream_id));
    if (it == _sessions.end()) {
        return nullptr;
    }
    auto session = it->second.lock();
    if (!session) {
        _sessions.erase(it);
    }
    return session;
}

std::shared_ptr<JT1078Session> JT1078SessionManager::findSessionByDevice(const std::string &sim, int channel) {
    std::lock_guard<std::mutex> lck(_mtx);
    auto device_it = _device_to_stream.find(makeDeviceKey(sim, channel));
    if (device_it == _device_to_stream.end()) {
        return nullptr;
    }

    auto session_it = _sessions.find(device_it->second);
    if (session_it == _sessions.end()) {
        _device_to_stream.erase(device_it);
        return nullptr;
    }

    auto session = session_it->second.lock();
    if (!session) {
        _sessions.erase(session_it);
        _device_to_stream.erase(device_it);
    }
    return session;
}

std::string JT1078SessionManager::makeStreamKey(const std::string &app, const std::string &stream_id) {
    return app + "/" + stream_id;
}

std::string JT1078SessionManager::makeDeviceKey(const std::string &sim, int channel) {
    return sim + "_" + std::to_string(channel);
}

} // namespace mediakit
