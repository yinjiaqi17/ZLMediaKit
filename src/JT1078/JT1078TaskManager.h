/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree.
 */

#ifndef ZLMEDIAKIT_JT1078TASKMANAGER_H
#define ZLMEDIAKIT_JT1078TASKMANAGER_H

#include <cstdint>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace mediakit {

enum class JT1078BizType {
    Live,
    Playback
};

struct JT1078Task {
    JT1078BizType biz_type = JT1078BizType::Live;
    std::string sim;
    int channel = 0;
    std::string app;
    std::string stream_id;
    std::string task_id;
    uint64_t create_ms = 0;
    uint64_t expire_ms = 0;
};

class JT1078TaskManager {
public:
    static JT1078TaskManager &Instance();

    bool addPendingTask(const JT1078Task &task);
    bool findPendingTask(const std::string &sim, int channel, JT1078Task &task);
    bool removePendingTask(const std::string &sim, int channel);
    bool removePendingTaskByStream(const std::string &app, const std::string &stream_id);
    size_t clearExpiredTasks();
    std::vector<JT1078Task> listPendingTasks();

    static const char *bizTypeToString(JT1078BizType type);

private:
    static std::string makeDeviceKey(const std::string &sim, int channel);
    static uint64_t nowMs();

private:
    std::mutex _mtx;
    std::unordered_map<std::string, JT1078Task> _tasks;
};

} // namespace mediakit

#endif // ZLMEDIAKIT_JT1078TASKMANAGER_H
