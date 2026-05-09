/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree.
 */

#include "JT1078TaskManager.h"
#include "Util/logger.h"
#include "Util/util.h"

using namespace toolkit;

namespace mediakit {

JT1078TaskManager &JT1078TaskManager::Instance() {
    static JT1078TaskManager instance;
    return instance;
}

bool JT1078TaskManager::addPendingTask(const JT1078Task &task) {
    if (task.sim.empty() || task.channel <= 0 || task.app.empty() || task.stream_id.empty()) {
        WarnL << "JT1078 task add failed, invalid task"
              << ", sim: " << (task.sim.empty() ? "-" : task.sim)
              << ", channel: " << task.channel
              << ", app: " << (task.app.empty() ? "-" : task.app)
              << ", stream_id: " << (task.stream_id.empty() ? "-" : task.stream_id);
        return false;
    }

    auto copy = task;
    if (!copy.create_ms) {
        copy.create_ms = nowMs();
    }
    if (!copy.expire_ms) {
        copy.expire_ms = copy.create_ms + 30 * 1000;
    }

    auto key = makeDeviceKey(copy.sim, copy.channel);
    std::lock_guard<std::mutex> lck(_mtx);
    _tasks[key] = copy;

    InfoL << "JT1078 task created"
          << ", biz_type: " << bizTypeToString(copy.biz_type)
          << ", sim: " << copy.sim
          << ", channel: " << copy.channel
          << ", app: " << copy.app
          << ", stream_id: " << copy.stream_id
          << ", task_id: " << (copy.task_id.empty() ? "-" : copy.task_id)
          << ", expire_ms: " << copy.expire_ms;
    return true;
}

bool JT1078TaskManager::findPendingTask(const std::string &sim, int channel, JT1078Task &task) {
    clearExpiredTasks();

    std::lock_guard<std::mutex> lck(_mtx);
    auto it = _tasks.find(makeDeviceKey(sim, channel));
    if (it == _tasks.end()) {
        return false;
    }
    task = it->second;
    return true;
}

bool JT1078TaskManager::removePendingTask(const std::string &sim, int channel) {
    std::lock_guard<std::mutex> lck(_mtx);
    auto key = makeDeviceKey(sim, channel);
    auto it = _tasks.find(key);
    if (it == _tasks.end()) {
        return false;
    }

    InfoL << "JT1078 task removed"
          << ", biz_type: " << bizTypeToString(it->second.biz_type)
          << ", sim: " << it->second.sim
          << ", channel: " << it->second.channel
          << ", app: " << it->second.app
          << ", stream_id: " << it->second.stream_id
          << ", task_id: " << (it->second.task_id.empty() ? "-" : it->second.task_id);
    _tasks.erase(it);
    return true;
}

bool JT1078TaskManager::removePendingTaskByStream(const std::string &app, const std::string &stream_id) {
    std::lock_guard<std::mutex> lck(_mtx);
    for (auto it = _tasks.begin(); it != _tasks.end(); ++it) {
        if (it->second.app == app && it->second.stream_id == stream_id) {
            InfoL << "JT1078 task removed by stream"
                  << ", biz_type: " << bizTypeToString(it->second.biz_type)
                  << ", sim: " << it->second.sim
                  << ", channel: " << it->second.channel
                  << ", app: " << it->second.app
                  << ", stream_id: " << it->second.stream_id
                  << ", task_id: " << (it->second.task_id.empty() ? "-" : it->second.task_id);
            _tasks.erase(it);
            return true;
        }
    }
    return false;
}

size_t JT1078TaskManager::clearExpiredTasks() {
    auto now = nowMs();
    size_t count = 0;
    std::lock_guard<std::mutex> lck(_mtx);
    for (auto it = _tasks.begin(); it != _tasks.end();) {
        if (it->second.expire_ms && it->second.expire_ms <= now) {
            WarnL << "JT1078 pending task expired"
                  << ", biz_type: " << bizTypeToString(it->second.biz_type)
                  << ", sim: " << it->second.sim
                  << ", channel: " << it->second.channel
                  << ", app: " << it->second.app
                  << ", stream_id: " << it->second.stream_id
                  << ", task_id: " << (it->second.task_id.empty() ? "-" : it->second.task_id);
            it = _tasks.erase(it);
            ++count;
            continue;
        }
        ++it;
    }
    return count;
}

std::vector<JT1078Task> JT1078TaskManager::listPendingTasks() {
    clearExpiredTasks();

    std::vector<JT1078Task> ret;
    std::lock_guard<std::mutex> lck(_mtx);
    ret.reserve(_tasks.size());
    for (auto &pr : _tasks) {
        ret.emplace_back(pr.second);
    }
    return ret;
}

const char *JT1078TaskManager::bizTypeToString(JT1078BizType type) {
    switch (type) {
        case JT1078BizType::Live:
            return "live";
        case JT1078BizType::Playback:
            return "playback";
        default:
            return "unknown";
    }
}

std::string JT1078TaskManager::makeDeviceKey(const std::string &sim, int channel) {
    return sim + "_" + std::to_string(channel);
}

uint64_t JT1078TaskManager::nowMs() {
    return getCurrentMillisecond();
}

} // namespace mediakit
