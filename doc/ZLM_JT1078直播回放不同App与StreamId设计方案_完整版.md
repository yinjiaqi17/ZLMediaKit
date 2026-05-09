# ZLMediaKit 侧 JT1078 直播 / 回放不同 app 与不同 streamId 设计方案

## 1. 目标

在 ZLMediaKit 改造版本中，支持 JT1078 实时直播和历史回放使用不同的 ZLM `app` 与不同的 `streamId`，避免直播和回放互相覆盖、串流、时间戳冲突或播放异常。

目标播放地址：

```text
直播：
http://127.0.0.1:80/live/{streamId}.live.flv

回放：
http://127.0.0.1:80/playback/{streamId}.live.flv
```

示例：

```text
直播：
http://127.0.0.1:80/live/013912345678_1_live.live.flv

回放：
http://127.0.0.1:80/playback/013912345678_1_replay_20260509103000123.live.flv
```

---

## 2. 核心结论

ZLM 本身无法仅通过 JT1078 RTP 包自动判断当前流是直播还是回放。

JT1078 RTP 包通常只包含：

```text
SIM
channel
dataType
packetType
sequence
timestamp
payload
```

它可以识别：

```text
哪个设备
哪个通道
音频还是视频
RTP 时间戳
```

但无法稳定识别：

```text
这是直播还是回放
是哪一个回放任务
对应哪个播放地址
```

因此需要业务平台在下发直播或回放指令前，提前通知 ZLM 创建或登记一个 pending task。ZLM 在收到终端推流后，通过第一个 RTP 包中的 `sim + channel` 命中 pending task，并将当前 TCP Session 绑定到对应的 `app + streamId`。

---

## 3. ZLM HTTP-FLV 地址规则

ZLM 的 HTTP-FLV 播放地址规则：

```text
http://{host}:{httpPort}/{app}/{streamId}.live.flv
```

其中：

| 字段 | 含义 | 示例 |
|---|---|---|
| host | ZLM HTTP 服务 IP | `127.0.0.1` |
| httpPort | ZLM HTTP 端口 | `80` |
| app | ZLM MediaSource app | `live` / `playback` |
| streamId | ZLM MediaSource stream | `013912345678_1_live` |

---

## 4. app 设计

### 4.1 直播 app

```text
app = live
```

直播播放地址：

```text
http://127.0.0.1:80/live/{streamId}.live.flv
```

### 4.2 回放 app

```text
app = playback
```

回放播放地址：

```text
http://127.0.0.1:80/playback/{streamId}.live.flv
```

### 4.3 后续扩展

如果后续需要监听和对讲，也可以继续扩展：

```text
监听 app = monitor
对讲 app = talk
```

播放地址示例：

```text
http://127.0.0.1:80/monitor/{streamId}.live.flv
http://127.0.0.1:80/talk/{streamId}.live.flv
```

---

## 5. streamId 设计

### 5.1 直播 streamId

直播建议使用固定 streamId：

```text
{sim}_{channel}_live
```

示例：

```text
013912345678_1_live
```

完整播放地址：

```text
http://127.0.0.1:80/live/013912345678_1_live.live.flv
```

### 5.2 回放 streamId

回放必须带 `taskId`：

```text
{sim}_{channel}_replay_{taskId}
```

示例：

```text
013912345678_1_replay_20260509103000123
```

完整播放地址：

```text
http://127.0.0.1:80/playback/013912345678_1_replay_20260509103000123.live.flv
```

### 5.3 为什么直播可以不带 taskId

直播是实时状态型业务，同一设备同一通道通常只有一路实时直播，因此可以用固定 streamId：

```text
013912345678_1_live
```

但是直播内部也可以有 `taskId`，用于业务审计、统计时长、开始/结束记录，只是不一定放到 streamId 里。

推荐直播任务结构：

```json
{
  "bizType": "LIVE",
  "taskId": "live_20260509103000123",
  "app": "live",
  "streamId": "013912345678_1_live"
}
```

### 5.4 为什么回放必须带 taskId

回放是任务型业务，包含：

```text
开始时间
结束时间
倍速
暂停
继续
拖动
停止
```

同一设备同一通道可能发起多次不同时间段的回放。如果不带 `taskId`，多个回放任务会互相覆盖，日志和状态也难以追踪。

---

## 6. ZLM 内部核心对象设计

### 6.1 业务类型枚举

```cpp
enum class JT1078BizType {
    LIVE,
    PLAYBACK,
    MONITOR,
    TALK
};
```

### 6.2 PendingTask

业务平台下发指令前，先通知 ZLM 创建 pending task。

```cpp
struct JT1078PendingTask {
    std::string sim;
    int channel = 0;

    JT1078BizType bizType = JT1078BizType::LIVE;

    std::string app;
    std::string streamId;
    std::string taskId;

    uint64_t createTimeMs = 0;
    uint64_t expireMs = 30000;
};
```

### 6.3 SessionContext

ZLM 收到终端推流后，需要将当前 TCP Session 绑定到一个固定业务上下文。

```cpp
struct JT1078SessionContext {
    std::string sim;
    int channel = 0;

    JT1078BizType bizType = JT1078BizType::LIVE;

    std::string app;
    std::string streamId;
    std::string taskId;

    bool bound = false;
};
```

绑定后，当前 Session 后续所有 RTP 包都必须写入同一个：

```text
app + streamId
```

不能每个包都重新查 `sim + channel`，否则直播和回放可能串流。

---

## 7. PendingTask 管理器设计

### 7.1 类结构

```cpp
class JT1078TaskManager {
public:
    static JT1078TaskManager &Instance();

    void addPendingTask(const JT1078PendingTask &task);

    std::optional<JT1078PendingTask> findPendingTask(
        const std::string &sim,
        int channel
    );

    void removePendingTask(
        const std::string &sim,
        int channel
    );

    void clearExpiredTasks();

private:
    std::string makeKey(const std::string &sim, int channel) const;

private:
    std::mutex _mtx;
    std::unordered_map<std::string, JT1078PendingTask> _pendingTasks;
};
```

### 7.2 key 设计

如果同一通道互斥，key 可以使用：

```text
{sim}_{channel}
```

示例：

```text
013912345678_1
```

如果后续支持同通道多个回放并发，则需要改成：

```text
{sim}_{channel}_{taskId}
```

或者使用独立 TCP 端口 / Session ID 绑定。

目前建议第一版使用：

```text
{sim}_{channel}
```

因为你的业务规则是同一设备同一通道只允许一个占用型业务。

---

## 8. 直播流程

### 8.1 业务平台流程

```text
前端点击直播
    |
    v
业务平台检查设备在线、通道状态
    |
    v
生成 taskId，可选
    |
    v
生成 app=live
    |
    v
生成 streamId={sim}_{channel}_live
    |
    v
调用 ZLM 创建 live pending task
    |
    v
下发 JT808 0x9101 实时音视频请求
    |
    v
返回直播播放地址
```

### 8.2 创建直播 pending task

请求：

```http
POST /index/api/jt1078/createLiveTask
```

请求体：

```json
{
  "sim": "013912345678",
  "channel": 1,
  "taskId": "live_20260509103000123",
  "app": "live",
  "streamId": "013912345678_1_live",
  "expireSec": 30
}
```

ZLM 保存：

```text
key = 013912345678_1

value = {
  bizType: LIVE,
  app: live,
  streamId: 013912345678_1_live,
  taskId: live_20260509103000123
}
```

### 8.3 ZLM 收流绑定

终端连接 ZLM 后，ZLM 收到第一个 RTP 包：

```text
sim=013912345678
channel=1
```

查 pending task：

```text
013912345678_1 -> LIVE
```

绑定当前 TCP Session：

```text
session -> app=live
session -> streamId=013912345678_1_live
session -> bizType=LIVE
```

创建 ZLM MediaSource：

```cpp
MediaTuple tuple{DEFAULT_VHOST, "live", "013912345678_1_live", ""};
```

播放地址：

```text
http://127.0.0.1:80/live/013912345678_1_live.live.flv
```

---

## 9. 回放流程

### 9.1 业务平台流程

```text
前端选择回放时间段
    |
    v
业务平台检查设备在线、通道状态
    |
    v
如果当前通道存在直播/监听/对讲/旧回放，先关闭旧业务
    |
    v
生成 replay taskId
    |
    v
生成 app=playback
    |
    v
生成 streamId={sim}_{channel}_replay_{taskId}
    |
    v
调用 ZLM 创建 playback pending task
    |
    v
下发 JT1078 回放请求
    |
    v
返回回放播放地址
```

### 9.2 创建回放 pending task

请求：

```http
POST /index/api/jt1078/createPlaybackTask
```

请求体：

```json
{
  "sim": "013912345678",
  "channel": 1,
  "taskId": "20260509103000123",
  "app": "playback",
  "streamId": "013912345678_1_replay_20260509103000123",
  "expireSec": 30
}
```

ZLM 保存：

```text
key = 013912345678_1

value = {
  bizType: PLAYBACK,
  app: playback,
  streamId: 013912345678_1_replay_20260509103000123,
  taskId: 20260509103000123
}
```

### 9.3 ZLM 收流绑定

终端连接 ZLM 后，ZLM 收到第一个 RTP 包：

```text
sim=013912345678
channel=1
```

查 pending task：

```text
013912345678_1 -> PLAYBACK
```

绑定当前 TCP Session：

```text
session -> app=playback
session -> streamId=013912345678_1_replay_20260509103000123
session -> bizType=PLAYBACK
session -> taskId=20260509103000123
```

创建 ZLM MediaSource：

```cpp
MediaTuple tuple{
    DEFAULT_VHOST,
    "playback",
    "013912345678_1_replay_20260509103000123",
    ""
};
```

播放地址：

```text
http://127.0.0.1:80/playback/013912345678_1_replay_20260509103000123.live.flv
```

---

## 10. JT1078StreamMuxer 改造

### 10.1 当前问题

如果 `JT1078StreamMuxer` 写死：

```cpp
MediaTuple tuple{DEFAULT_VHOST, "jt1078", _stream_id, ""};
```

则所有 JT1078 流都会变成：

```text
/jt1078/{streamId}.live.flv
```

无法满足：

```text
/live/{streamId}.live.flv
/playback/{streamId}.live.flv
```

### 10.2 改造目标

让 `JT1078StreamMuxer` 支持动态 app：

```cpp
bool start(const std::string &app, const std::string &stream_id);
```

### 10.3 头文件改造

```cpp
class JT1078StreamMuxer {
public:
    using Ptr = std::shared_ptr<JT1078StreamMuxer>;

    bool start(const std::string &app, const std::string &stream_id);

    bool inputFrame(const JT1078PsDemuxer::Frame &frame, uint64_t fallback_timestamp);

    void reset();

    bool started() const;

    const std::string &app() const;

    const std::string &streamId() const;

private:
    std::string _app;
    std::string _stream_id;
    MultiMediaSourceMuxer::Ptr _muxer;
};
```

### 10.4 cpp 改造

```cpp
bool JT1078StreamMuxer::start(
        const std::string &app,
        const std::string &stream_id
) {
    if (app.empty() || stream_id.empty()) {
        WarnL << "JT1078 step6 start muxer failed"
              << ", app: " << app
              << ", stream_id: " << stream_id;
        return false;
    }

    if (_muxer && _app == app && _stream_id == stream_id) {
        return true;
    }

    reset();

    _app = app;
    _stream_id = stream_id;

    ProtocolOption option;
    MediaTuple tuple{DEFAULT_VHOST, _app, _stream_id, ""};

    _muxer = std::make_shared<MultiMediaSourceMuxer>(
        tuple,
        0.0f,
        option
    );

    InfoL << "JT1078 step6 muxer_started"
          << ", app: " << _app
          << ", stream_id: " << _stream_id
          << ", short_url: " << tuple.shortUrl();

    return true;
}
```

### 10.5 reset

```cpp
void JT1078StreamMuxer::reset() {
    if (_muxer) {
        InfoL << "JT1078 step6 muxer_reset"
              << ", app: " << (_app.empty() ? "-" : _app)
              << ", stream_id: " << (_stream_id.empty() ? "-" : _stream_id);
    }

    _muxer.reset();
    _app.clear();
    _stream_id.clear();
}
```

---

## 11. JT1078Session 绑定逻辑

### 11.1 Session 初始化

每个 TCP 连接创建一个 `JT1078SessionContext`：

```cpp
class JT1078Session {
private:
    JT1078SessionContext _context;
    JT1078StreamMuxer _streamMuxer;
};
```

### 11.2 收到 RTP 包后绑定

```cpp
void JT1078Session::onRtpPacket(const JT1078RtpPacket &packet) {
    if (!_context.bound) {
        bindSession(packet.sim, packet.channel);
    }

    if (!_streamMuxer.started()) {
        _streamMuxer.start(_context.app, _context.streamId);
    }

    // 后续流程：
    // RTP payload -> FrameAssembler -> PsDemuxer -> StreamMuxer.inputFrame()
}
```

### 11.3 bindSession

```cpp
void JT1078Session::bindSession(
        const std::string &sim,
        int channel
) {
    _context.sim = sim;
    _context.channel = channel;

    auto taskOpt = JT1078TaskManager::Instance().findPendingTask(sim, channel);

    if (taskOpt.has_value()) {
        auto task = taskOpt.value();

        _context.bizType = task.bizType;
        _context.app = task.app;
        _context.streamId = task.streamId;
        _context.taskId = task.taskId;
        _context.bound = true;

        JT1078TaskManager::Instance().removePendingTask(sim, channel);

        InfoL << "JT1078 session bound pending task"
              << ", sim: " << sim
              << ", channel: " << channel
              << ", app: " << _context.app
              << ", stream_id: " << _context.streamId
              << ", task_id: " << _context.taskId;

        return;
    }

    // 没有 pending task 时，默认按直播处理。
    _context.bizType = JT1078BizType::LIVE;
    _context.app = "live";
    _context.streamId = sim + "_" + std::to_string(channel) + "_live";
    _context.taskId = "";
    _context.bound = true;

    InfoL << "JT1078 session bound default live"
          << ", sim: " << sim
          << ", channel: " << channel
          << ", app: " << _context.app
          << ", stream_id: " << _context.streamId;
}
```

---

## 12. API 设计

### 12.1 创建直播任务

```http
POST /index/api/jt1078/createLiveTask
```

请求：

```json
{
  "sim": "013912345678",
  "channel": 1,
  "taskId": "live_20260509103000123",
  "app": "live",
  "streamId": "013912345678_1_live",
  "expireSec": 30
}
```

返回：

```json
{
  "code": 0,
  "msg": "success",
  "data": {
    "app": "live",
    "streamId": "013912345678_1_live",
    "playUrl": "http://127.0.0.1:80/live/013912345678_1_live.live.flv"
  }
}
```

### 12.2 创建回放任务

```http
POST /index/api/jt1078/createPlaybackTask
```

请求：

```json
{
  "sim": "013912345678",
  "channel": 1,
  "taskId": "20260509103000123",
  "app": "playback",
  "streamId": "013912345678_1_replay_20260509103000123",
  "expireSec": 30
}
```

返回：

```json
{
  "code": 0,
  "msg": "success",
  "data": {
    "app": "playback",
    "streamId": "013912345678_1_replay_20260509103000123",
    "playUrl": "http://127.0.0.1:80/playback/013912345678_1_replay_20260509103000123.live.flv"
  }
}
```

### 12.3 查询 pending task

```http
GET /index/api/jt1078/getPendingTaskList
```

返回：

```json
{
  "code": 0,
  "data": [
    {
      "sim": "013912345678",
      "channel": 1,
      "bizType": "PLAYBACK",
      "app": "playback",
      "streamId": "013912345678_1_replay_20260509103000123",
      "taskId": "20260509103000123",
      "createTimeMs": 1778322600000,
      "expireMs": 30000
    }
  ]
}
```

### 12.4 关闭流

```http
POST /index/api/jt1078/closeStream
```

请求：

```json
{
  "app": "live",
  "streamId": "013912345678_1_live"
}
```

或者：

```json
{
  "app": "playback",
  "streamId": "013912345678_1_replay_20260509103000123"
}
```

---

## 13. 播放地址生成

### 13.1 ZLM 侧生成

```cpp
std::string buildFlvUrl(
        const std::string &host,
        int httpPort,
        const std::string &app,
        const std::string &streamId
) {
    return "http://" + host + ":" + std::to_string(httpPort)
           + "/" + app
           + "/" + streamId
           + ".live.flv";
}
```

### 13.2 Java 业务平台生成

```java
public String buildFlvUrl(String host, int httpPort, String app, String streamId) {
    return "http://" + host + ":" + httpPort + "/" + app + "/" + streamId + ".live.flv";
}
```

示例：

```java
String liveUrl = buildFlvUrl(
        "127.0.0.1",
        80,
        "live",
        "013912345678_1_live"
);
```

```java
String playbackUrl = buildFlvUrl(
        "127.0.0.1",
        80,
        "playback",
        "013912345678_1_replay_20260509103000123"
);
```

---

## 14. 直播和回放同通道冲突处理

根据业务规则：

```text
同一设备同一通道只允许一种占用型业务。
开启回放时，如果当前通道正在直播，需要先关闭直播。
```

因此流程为：

```text
通道 1 正在直播
    |
    v
用户发起通道 1 回放
    |
    v
业务平台关闭直播
    |
    v
业务平台调用 ZLM closeStream(app=live, streamId=xxx_live)
    |
    v
业务平台创建 playback pending task
    |
    v
业务平台下发回放指令
    |
    v
ZLM 收到回放流后绑定到 app=playback
```

这样直播播放地址：

```text
/live/013912345678_1_live.live.flv
```

会停止。

回放播放地址：

```text
/playback/013912345678_1_replay_20260509103000123.live.flv
```

会启动。

两者不会共用同一个 MediaSource。

---

## 15. 同端口与不同端口方案

### 15.1 不同端口方案

```text
直播端口：1078
回放端口：1079
监听端口：1080
对讲端口：1081
```

优点：

```text
实现简单
ZLM 可以按本地端口判断业务类型
不容易串流
```

缺点：

```text
需要终端支持下发不同端口
端口规划较多
```

### 15.2 同端口 pending task 方案

所有业务都推到同一个端口：

```text
1078
```

通过 pending task 区分：

```text
业务平台先通知 ZLM
ZLM 收到第一个 RTP 包后按 sim/channel 绑定任务
```

优点：

```text
端口少
更灵活
适合业务平台统一调度
```

缺点：

```text
必须维护 pending task
必须处理任务过期
必须处理同通道并发冲突
```

### 15.3 推荐

当前项目建议使用：

```text
同端口 + pending task 绑定
```

因为你已经有通道互斥规则，同一设备同一通道同一时间只会有一个占用业务。

---

## 16. 任务过期处理

pending task 必须有过期时间，防止业务平台下发命令后终端没有推流，导致 ZLM 状态残留。

建议默认：

```text
expireSec = 30
```

清理逻辑：

```cpp
void JT1078TaskManager::clearExpiredTasks() {
    auto now = getCurrentMillisecond();

    std::lock_guard<std::mutex> lck(_mtx);

    for (auto it = _pendingTasks.begin(); it != _pendingTasks.end();) {
        auto &task = it->second;

        if (now - task.createTimeMs > task.expireMs) {
            WarnL << "JT1078 pending task expired"
                  << ", sim: " << task.sim
                  << ", channel: " << task.channel
                  << ", app: " << task.app
                  << ", stream_id: " << task.streamId
                  << ", task_id: " << task.taskId;

            it = _pendingTasks.erase(it);
        } else {
            ++it;
        }
    }
}
```

可以在定时器里每 5 秒执行一次。

---

## 17. 重要注意事项

### 17.1 绑定后不要再动态切换

一旦 TCP Session 绑定：

```text
session -> app
session -> streamId
```

后续所有 RTP 包都必须写入这个 MediaSource。

错误做法：

```text
每个 RTP 包都重新查 sim/channel 当前任务
```

这会造成串流。

### 17.2 直播和回放不能使用同一个 streamId

错误：

```text
live/013912345678_1.live.flv
playback/013912345678_1.live.flv
```

虽然 app 不同可以区分，但日志、状态、任务管理不清晰。

推荐：

```text
live/013912345678_1_live.live.flv
playback/013912345678_1_replay_20260509103000123.live.flv
```

### 17.3 回放必须带 taskId

回放涉及时间段、暂停、继续、拖动、倍速、停止，必须有任务 ID 追踪生命周期。

### 17.4 直播可以有 taskId，但 streamId 建议固定

推荐：

```text
taskId = live_20260509103000123
streamId = 013912345678_1_live
```

这样前端直播地址稳定，后端仍然可以记录任务。

### 17.5 业务互斥应放在平台层

ZLM 只负责媒体：

```text
接收 RTP
解析 PS/H264
创建 MediaSource
关闭 MediaSource
```

业务平台负责：

```text
通道状态
业务互斥
抢占规则
toast 提示
指令下发
任务状态
```

---


---

## 18. ZLM 关闭流 / 关闭任务处理

### 18.1 关闭场景

ZLM 需要支持业务平台主动关闭任务或关闭流，包括：

```text
直播关闭
回放关闭
监听关闭
对讲关闭
pending task 还没推流时取消任务
active session 正在推流时强制关闭
```

业务平台在做通道互斥时，会主动调用 ZLM 的关闭接口。

例如：

```text
开启回放前，业务平台先关闭直播
开启监听前，业务平台先关闭直播/回放
开启对讲前，业务平台先关闭直播/回放/监听
```

ZLM 不负责判断是否抢占，只负责执行明确的关闭动作。

---

### 18.2 closeStream 接口

```http
POST /index/api/jt1078/closeStream
```

直播关闭请求：

```json
{
  "app": "live",
  "streamId": "013912345678_1_live",
  "sim": "013912345678",
  "channel": 1,
  "taskId": "live_20260509103000123"
}
```

回放关闭请求：

```json
{
  "app": "playback",
  "streamId": "013912345678_1_replay_20260509103000123",
  "sim": "013912345678",
  "channel": 1,
  "taskId": "20260509103000123"
}
```

响应：

```json
{
  "code": 0,
  "msg": "success"
}
```

---

### 18.3 closeTask 接口，可选

如果业务侧更习惯按任务关闭，也可以额外提供：

```http
POST /index/api/jt1078/closeTask
```

请求：

```json
{
  "sim": "013912345678",
  "channel": 1,
  "taskId": "20260509103000123"
}
```

ZLM 内部可以通过 `taskId` 找到对应的：

```text
app
streamId
session
pendingTask
```

然后复用 `closeStream` 的关闭逻辑。

第一版可以只实现 `closeStream`，因为业务平台一般已经知道 `app + streamId`。

---

### 18.4 ZLM 需要清理的资源

ZLM 收到 `closeStream` 或 `closeTask` 后，需要同时处理三类资源：

```text
1. pendingTask
2. active Session
3. MediaSource / StreamMuxer
```

#### pendingTask

pendingTask 表示业务平台已经调用：

```text
createLiveTask
createPlaybackTask
```

但终端还没有推流。

此时 ZLM 只有等待任务，还没有 MediaSource，也没有 active session。

关闭时只需要：

```text
删除 pendingTask
返回 success
```

#### active Session

active Session 表示终端已经推流进来，ZLM 已经完成：

```text
TCP Session 绑定
StreamMuxer 创建
MediaSource 注册
```

关闭时需要：

```text
关闭 JT1078Session
reset StreamMuxer
注销 MediaSource
删除 SessionManager 映射
返回 success
```

#### MediaSource / StreamMuxer

ZLM 的 MediaSource 通常由 `MultiMediaSourceMuxer` 生命周期管理。

当业务主动关闭时，需要调用：

```cpp
_streamMuxer.reset();
```

让内部 `MultiMediaSourceMuxer` 析构，从而注销对应的 MediaSource。

---

### 18.5 推荐关闭顺序

ZLM 收到关闭请求后，推荐按以下顺序处理：

```text
1. 删除 pendingTask
2. 查找 active session
3. 如果 session 存在，关闭 session
4. reset StreamMuxer
5. 注销 MediaSource
6. 删除 SessionManager 映射
7. 返回 success
```

这样可以同时兼容两种场景：

```text
场景一：任务还没推流，只存在 pendingTask
场景二：任务已经推流，存在 active session 和 MediaSource
```

---

### 18.6 closeStream 伪代码

```cpp
Json::Value JT1078Api::closeStream(const Json::Value &req) {
    auto app = req["app"].asString();
    auto streamId = req["streamId"].asString();
    auto sim = req["sim"].asString();
    auto channel = req["channel"].asInt();

    // 1. 删除 pending task。
    // 如果任务还没推流，这一步就能完成关闭。
    if (!sim.empty() && channel > 0) {
        JT1078TaskManager::Instance().removePendingTask(sim, channel);
    }

    // 2. 查找 active session。
    auto session = JT1078SessionManager::Instance().findSession(app, streamId);

    // 3. 如果 session 存在，关闭 session。
    if (session) {
        session->closeByBiz("close stream api");
    }

    Json::Value res;
    res["code"] = 0;
    res["msg"] = "success";
    return res;
}
```

---

### 18.7 JT1078Session 主动关闭逻辑

```cpp
void JT1078Session::closeByBiz(const std::string &reason) {
    WarnL << "JT1078 session close by biz"
          << ", app: " << _context.app
          << ", stream_id: " << _context.streamId
          << ", task_id: " << _context.taskId
          << ", reason: " << reason;

    // 1. 停止媒体输出，注销 MediaSource。
    _streamMuxer.reset();

    // 2. 删除 SessionManager 映射。
    JT1078SessionManager::Instance().removeSession(
        _context.app,
        _context.streamId
    );

    // 3. 主动关闭 TCP 连接。
    if (_sock) {
        _sock->shutdown(SockException(Err_shutdown, reason));
    }

    // 4. 清理上下文。
    _context.bound = false;
}
```

注意：

```text
closeByBiz 是业务主动关闭。
普通网络断开走 onClose。
两者都应该清理 SessionManager 和 StreamMuxer。
```

---

### 18.8 JT1078Session onClose 清理逻辑

即使不是业务主动关闭，而是终端断开 TCP，也必须清理资源。

```cpp
void JT1078Session::onClose(const SockException &ex) {
    WarnL << "JT1078 session closed"
          << ", app: " << _context.app
          << ", stream_id: " << _context.streamId
          << ", reason: " << ex.what();

    _streamMuxer.reset();

    if (_context.bound) {
        JT1078SessionManager::Instance().removeSession(
            _context.app,
            _context.streamId
        );
    }

    _context.bound = false;
}
```

---

### 18.9 JT1078StreamMuxer reset

`JT1078StreamMuxer` 需要支持动态 app 和 streamId，因此 reset 时要清理：

```text
app
streamId
muxer
track 状态
H264 缓存状态
```

示例：

```cpp
void JT1078StreamMuxer::reset() {
    if (_muxer) {
        InfoL << "JT1078 muxer reset"
              << ", app: " << (_app.empty() ? "-" : _app)
              << ", stream_id: " << (_stream_id.empty() ? "-" : _stream_id);
    }

    _muxer.reset();

    _app.clear();
    _stream_id.clear();

    _track_added.clear();
    _track_completed = false;

    _h264_sps_input = false;
    _h264_pps_input = false;
    clearH264AccessUnit();

    _last_dts = 0;
    _last_pts = 0;
}
```

---

### 18.10 SessionManager 设计

ZLM 需要维护 active session 映射。

Key：

```text
{app}/{streamId}
```

示例：

```text
live/013912345678_1_live
playback/013912345678_1_replay_20260509103000123
```

类结构：

```cpp
class JT1078SessionManager {
public:
    static JT1078SessionManager &Instance();

    void addSession(
        const std::string &app,
        const std::string &streamId,
        const std::weak_ptr<JT1078Session> &session
    );

    std::shared_ptr<JT1078Session> findSession(
        const std::string &app,
        const std::string &streamId
    );

    void removeSession(
        const std::string &app,
        const std::string &streamId
    );

private:
    std::string makeKey(
        const std::string &app,
        const std::string &streamId
    ) {
        return app + "/" + streamId;
    }

private:
    std::mutex _mtx;
    std::unordered_map<std::string, std::weak_ptr<JT1078Session>> _sessions;
};
```

当 session 绑定成功后注册：

```cpp
JT1078SessionManager::Instance().addSession(
    _context.app,
    _context.streamId,
    shared_from_this()
);
```

当业务主动关闭或 TCP 断开时删除：

```cpp
JT1078SessionManager::Instance().removeSession(
    _context.app,
    _context.streamId
);
```

---

### 18.11 关闭直播再开启回放的完整 ZLM 处理

业务侧执行：

```text
1. closeStream(app=live, streamId=013912345678_1_live)
2. createPlaybackTask(app=playback, streamId=013912345678_1_replay_xxx)
3. 下发回放指令
```

ZLM 处理：

```text
closeStream:
    删除 live pendingTask
    查找 live active session
    如果存在，关闭 session
    reset live StreamMuxer
    注销 /live/013912345678_1_live

createPlaybackTask:
    保存 playback pendingTask

终端推流:
    收到第一个 RTP
    解析 sim/channel
    命中 playback pendingTask
    当前 session 绑定 app=playback, streamId=xxx_replay_xxx
    创建 /playback/xxx_replay_xxx
```

最终效果：

```text
直播地址失效：
http://127.0.0.1:80/live/013912345678_1_live.live.flv

回放地址生效：
http://127.0.0.1:80/playback/013912345678_1_replay_20260509103000123.live.flv
```

---

### 18.12 关闭接口必须幂等

`closeStream` 应该是幂等的。

也就是说，多次调用同一个关闭请求，都应该返回成功：

```text
第一次调用：真正关闭 session / pendingTask / MediaSource
第二次调用：资源已经不存在，也返回 success
```

不要因为资源不存在就返回错误。

推荐日志：

```cpp
if (!session) {
    InfoL << "JT1078 close stream, active session not found"
          << ", app: " << app
          << ", stream_id: " << streamId;
}
```

响应仍然：

```json
{
  "code": 0,
  "msg": "success"
}
```

---

### 18.13 前端效果

关闭直播后：

```text
/live/{streamId}.live.flv
```

会断开。

前端需要根据业务侧返回的抢占结果关闭播放器：

```js
if (res.preempted && res.closedBizType === 'LIVE') {
  closeLivePlayer(deviceId, channel);
}
```

如果前端没有主动销毁播放器，flv.js 可能会报：

```text
MediaMSEError
appendBuffer failed
ring buffer detached
```

这是因为流已经被 ZLM 注销，播放器仍在尝试读取数据。


## 19. 最终效果

### 18.1 直播

ZLM 创建：

```text
app = live
streamId = 013912345678_1_live
```

播放：

```text
http://127.0.0.1:80/live/013912345678_1_live.live.flv
```

### 18.2 回放

ZLM 创建：

```text
app = playback
streamId = 013912345678_1_replay_20260509103000123
```

播放：

```text
http://127.0.0.1:80/playback/013912345678_1_replay_20260509103000123.live.flv
```

### 18.3 通道抢占

如果通道正在直播，用户开启回放：

```text
直播关闭
回放启动
前端 toast：回放请求成功，通道被占用，视频直播已关闭
```

直播地址失效：

```text
http://127.0.0.1:80/live/013912345678_1_live.live.flv
```

回放地址生效：

```text
http://127.0.0.1:80/playback/013912345678_1_replay_20260509103000123.live.flv
```

---

## 20. 总结

ZLM 侧实现直播和回放不同地址的关键是：

```text
不同 app
不同 streamId
Session 绑定 pending task
```

最终规则：

```text
直播：
app=live
streamId={sim}_{channel}_live

回放：
app=playback
streamId={sim}_{channel}_replay_{taskId}
```

播放地址：

```text
直播：
/live/{streamId}.live.flv

回放：
/playback/{streamId}.live.flv
```

ZLM 不主动判断直播/回放，而是由业务平台提前创建 pending task，ZLM 收到 RTP 后绑定当前 TCP Session 到对应的 app 和 streamId。
