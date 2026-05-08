# ZLMediaKit 改造支持 JT1078 直播、回放、语音对讲方案

## 1. 改造目标

在 ZLMediaKit 基础上新增 JT/T 1078 相关能力：

1. **JT1078 实时视频直播**
   - 终端通过 JT1078 RTP 推送实时音视频。
   - ZLM 接收、解析、解复用后输出 HTTP-FLV / WS-FLV / RTMP / RTSP / WebRTC。

2. **JT1078 历史视频回放**
   - 业务平台通过 JT808/1078 控制指令请求终端回放历史录像。
   - 终端推送历史 RTP 流到 ZLM。
   - ZLM 生成独立回放流供前端播放。

3. **JT1078 语音对讲**
   - 浏览器或 App 采集音频。
   - 平台/ZLM 转为终端支持的音频格式，例如 G711A。
   - 封装为 JT1078 RTP 音频包发送给终端。
   - 同时支持终端音频回传给浏览器或 App。

---

## 2. 总体架构

```text
前端 Web / App
   |
   | HTTP-FLV / WS-FLV / RTMP / RTSP / WebRTC
   v
ZLMediaKit
   |
   | 新增 JT1078 接入模块
   | RTP 解包 / 分包重组 / PS 解复用 / H264/H265/G711 提取
   v
JT1078 TCP/UDP Server
   ^
   |
车载终端


业务平台 / JT808 网关
   |
   | 0x9101 实时音视频请求
   | 0x9201 远程录像回放请求
   | 0x9202 远程录像回放控制
   | 语音对讲控制
   v
车载终端
```

---

## 3. 职责边界

### 3.1 业务平台 / JT808 网关负责

1. 设备注册、鉴权、心跳。
2. 设备在线状态维护。
3. JT808 消息编码、解码。
4. 下发实时视频请求，例如 `0x9101`。
5. 下发历史回放请求，例如 `0x9201`。
6. 下发回放控制，例如暂停、继续、拖动、倍速、停止。
7. 下发语音对讲控制。
8. 维护指令流水号和请求响应关系。
9. 管理车辆、通道、用户权限。
10. 生成前端播放地址。

### 3.2 ZLMediaKit 负责

1. JT1078 TCP/UDP 数据接入。
2. JT1078 RTP 包解析。
3. RTP 分包重组。
4. MPEG-PS 解复用。
5. H264/H265 视频帧提取。
6. G711A/G711U 音频帧提取。
7. 注册 ZLM 内部 MediaSource。
8. 输出 HTTP-FLV / WS-FLV / RTMP / RTSP / WebRTC。
9. 流生命周期管理。
10. 回放流任务管理。
11. 语音对讲音频收发。

---

## 4. 改造原则

### 4.1 不建议 ZLM 处理 JT808 信令

ZLM 不负责：

```text
设备注册
终端鉴权
心跳
0x9101 下发
0x9201 下发
0x9202 回放控制
设备通道查询
车辆状态
权限控制
```

这些保持在 Spring Boot + Netty 的 JT808 网关中。

### 4.2 ZLM 只新增 JT1078 媒体接入层

ZLM 新增能力：

```text
JT1078 TCP/UDP 监听
JT1078 RTP 包解析
JT1078 RTP 分包重组
MPEG-PS 解复用
H264/H265 视频帧提取
G711A/G711U 音频帧提取
注册 ZLM 内部流
输出 HTTP-FLV / WS-FLV / RTMP / RTSP / WebRTC
```

### 4.3 优先复用 ZLM 现有媒体分发能力

不要自己重写：

```text
FLV 输出
RTMP 输出
RTSP 输出
WebRTC 输出
录制
截图
MediaSource 管理
播放器协议转换
```

改造重点放在：

```text
JT1078 协议解析
RTP 重组
PS Demux
H264/H265/G711 适配 ZLM 内部媒体轨道
```

---

## 5. 源码目录规划

建议在 ZLM 源码中新增目录：

```text
ZLMediaKit
└── src
    └── JT1078
        ├── JT1078TcpServer.h
        ├── JT1078TcpServer.cpp
        ├── JT1078UdpServer.h
        ├── JT1078UdpServer.cpp
        ├── JT1078Session.h
        ├── JT1078Session.cpp
        ├── JT1078RtpPacket.h
        ├── JT1078RtpPacket.cpp
        ├── JT1078RtpDecoder.h
        ├── JT1078RtpDecoder.cpp
        ├── JT1078FrameAssembler.h
        ├── JT1078FrameAssembler.cpp
        ├── JT1078PsDemuxer.h
        ├── JT1078PsDemuxer.cpp
        ├── JT1078StreamMuxer.h
        ├── JT1078StreamMuxer.cpp
        ├── JT1078ReplayManager.h
        ├── JT1078ReplayManager.cpp
        ├── JT1078TalkSession.h
        ├── JT1078TalkSession.cpp
        ├── JT1078AudioCodec.h
        ├── JT1078AudioCodec.cpp
        ├── JT1078Api.h
        └── JT1078Api.cpp
```

---

## 6. CMake 接入

### 6.1 增加 JT1078 源文件

在 ZLM 的 CMake 构建配置中加入：

```cmake
file(GLOB JT1078_SRC
    src/JT1078/*.cpp
)
```

如果 MediaServer 使用 `target_sources`，则增加：

```cmake
target_sources(MediaServer PRIVATE
    ${JT1078_SRC}
)
```

如果当前工程是手动维护源码列表，则追加：

```cmake
src/JT1078/JT1078TcpServer.cpp
src/JT1078/JT1078Session.cpp
src/JT1078/JT1078RtpDecoder.cpp
src/JT1078/JT1078FrameAssembler.cpp
src/JT1078/JT1078PsDemuxer.cpp
src/JT1078/JT1078StreamMuxer.cpp
src/JT1078/JT1078ReplayManager.cpp
src/JT1078/JT1078TalkSession.cpp
src/JT1078/JT1078AudioCodec.cpp
src/JT1078/JT1078Api.cpp
```

---

## 7. 配置文件设计

在 `config.ini` 增加：

```ini
[jt1078]
enable=1
port=1078
transport=tcp
timeoutSec=30
waitIFrame=1
reorderBufferSize=64
maxPacketCache=1048576
streamApp=jt1078

[jt1078.replay]
enable=1
timeoutSec=60

[jt1078.talk]
enable=1
port=1079
codec=G711A
sampleRate=8000
channels=1
frameDurationMs=20
```

字段说明：

| 配置项 | 说明 |
|---|---|
| `enable` | 是否启用 JT1078 接入 |
| `port` | JT1078 TCP/UDP 监听端口 |
| `transport` | 传输方式，第一版建议先支持 TCP |
| `timeoutSec` | 会话超时时间 |
| `waitIFrame` | 是否等待 I 帧后再输出 |
| `reorderBufferSize` | RTP 乱序缓存大小 |
| `maxPacketCache` | 单帧最大缓存限制 |
| `streamApp` | ZLM 内部流 app 名称 |
| `codec` | 默认对讲音频编码 |
| `sampleRate` | 对讲采样率 |
| `channels` | 对讲声道数 |
| `frameDurationMs` | 对讲音频帧时长 |

---

## 8. 第一阶段：JT1078 实时直播

### 8.1 阶段目标

实现：

```text
终端推送 JT1078 RTP
ZLM 接收 RTP
ZLM 解析 PS/H264
ZLM 注册内部流
前端通过 HTTP-FLV / WS-FLV 播放
```

### 8.2 步骤 1：新增 JT1078TcpServer

目标：

```text
ZLM 启动后监听 1078 端口
终端可以 TCP 连接进来
ZLM 能打印连接 IP、端口、收包长度
```

启动流程：

```text
MediaServer 启动
    |
    v
读取 config.ini [jt1078]
    |
    v
enable=1
    |
    v
启动 JT1078TcpServer
    |
    v
accept 终端连接
    |
    v
创建 JT1078Session
```

验收：

```bash
telnet 127.0.0.1 1078
```

或者使用 1078 模拟器连接后，ZLM 日志能看到：

```text
JT1078 client connected: 127.0.0.1:xxxxx
```

### 8.3 步骤 2：实现 JT1078Session

`JT1078Session` 负责每个终端连接的数据处理：

```cpp
class JT1078Session {
public:
    void onRecv(const char* data, size_t len);
    void onError(const std::exception& ex);
    void onClose();

private:
    std::string sim_;
    int channel_ = 0;
    std::string streamId_;

    JT1078RtpDecoder rtpDecoder_;
    JT1078FrameAssembler frameAssembler_;
    JT1078PsDemuxer psDemuxer_;
    JT1078StreamMuxer streamMuxer_;
};
```

职责：

```text
接收 TCP 字节流
拆 JT1078 RTP 包
解析 SIM / 通道 / 数据类型 / 分包标记
重组完整 PS 帧
PS 解复用
送入 ZLM 内部流
```

### 8.4 步骤 3：实现 JT1078 RTP 解析

JT1078 RTP 一般包括：

```text
RTP Header
+
SSRC
+
JT1078 扩展头
+
Payload
```

至少需要解析：

```text
V/P/X/CC
M/PT
sequence
timestamp
ssrc
sim
channel
dataType
packetType
payload
```

建议结构：

```cpp
struct JT1078RtpPacket {
    uint16_t sequence = 0;
    uint32_t timestamp = 0;
    uint32_t ssrc = 0;

    std::string sim;
    uint8_t channel = 0;
    uint8_t dataType = 0;
    uint8_t packetType = 0;

    bool marker = false;
    bool keyFrame = false;

    const uint8_t* payload = nullptr;
    size_t payloadSize = 0;
};
```

数据类型建议：

```text
0x00 视频 I 帧
0x01 视频 P 帧
0x02 视频 B 帧
0x03 音频帧
0x04 透传数据
```

分包类型建议：

```text
00 原子包
01 第一包
10 最后一包
11 中间包
```

### 8.5 步骤 4：实现 RTP 分包重组

很多终端的视频不是一个 RTP 包一个完整 PS，需要重组：

```text
原子包 -> 直接输出
第一包 -> 创建缓存
中间包 -> 追加缓存
最后包 -> 追加缓存并输出完整帧
```

伪代码：

```cpp
bool JT1078FrameAssembler::input(const JT1078RtpPacket& pkt, Buffer::Ptr& out) {
    switch (pkt.packetType) {
        case ATOM:
            out = makeBuffer(pkt.payload, pkt.payloadSize);
            return true;

        case FIRST:
            cache_.clear();
            cache_.append(pkt.payload, pkt.payloadSize);
            return false;

        case MIDDLE:
            cache_.append(pkt.payload, pkt.payloadSize);
            return false;

        case LAST:
            cache_.append(pkt.payload, pkt.payloadSize);
            out = cache_.toBuffer();
            cache_.clear();
            return true;
    }

    return false;
}
```

必须增加保护：

```text
sequence 连续性检查
缓存大小限制
丢包后丢弃当前帧
异常包保护
乱序包缓冲
等待 I 帧策略
```

### 8.6 步骤 5：实现 PS 解复用

大多数 1078 视频流是：

```text
JT1078 RTP Payload -> MPEG-PS -> PES -> H264/H265
```

PS 解复用需要识别：

```text
0x000001BA  Pack Header
0x000001BB  System Header
0x000001BC  Program Stream Map
0x000001E0  Video PES
0x000001C0  Audio PES
```

第一版建议只支持：

```text
视频：H264
音频：G711A
```

视频提取目标：

```text
PS -> PES payload -> H264 NALU
```

然后送入 ZLM 的 H264 Track。

### 8.7 步骤 6：接入 ZLM MediaSource

不要自己写 FLV/RTMP/WebRTC 输出，而是把 H264/G711 帧注册到 ZLM 内部流。

流命名建议：

```text
app = jt1078
stream = {sim}_{channel}_live
```

示例：

```text
app    = jt1078
stream = 013912345678_1_live
```

播放地址示例：

```text
HTTP-FLV:
http://127.0.0.1:8080/jt1078/013912345678_1_live.live.flv

WS-FLV:
ws://127.0.0.1:8080/jt1078/013912345678_1_live.live.flv

RTSP:
rtsp://127.0.0.1:554/jt1078/013912345678_1_live

RTMP:
rtmp://127.0.0.1:1935/jt1078/013912345678_1_live

WebRTC:
http://127.0.0.1:8080/index/api/webrtc?app=jt1078&stream=013912345678_1_live&type=play
```

### 8.8 步骤 7：业务平台下发 0x9101

直播不是 ZLM 主动拉流，而是：

```text
前端点击播放
    |
    v
业务平台生成 streamId
    |
    v
808 网关下发 0x9101 实时音视频传输请求
    |
    v
终端连接 ZLM 1078 端口并推流
    |
    v
ZLM 出播放地址
```

业务平台接口建议：

```http
POST /vehicle/video/live/start
```

请求：

```json
{
  "deviceId": "013912345678",
  "channel": 1,
  "streamType": 0,
  "dataType": 0,
  "transport": "tcp"
}
```

返回：

```json
{
  "code": 0,
  "streamId": "013912345678_1_live",
  "flvUrl": "http://127.0.0.1:8080/jt1078/013912345678_1_live.live.flv",
  "wsFlvUrl": "ws://127.0.0.1:8080/jt1078/013912345678_1_live.live.flv"
}
```

---

## 9. 第二阶段：JT1078 历史回放

### 9.1 回放本质

回放和直播在媒体层基本一致：

```text
终端推 RTP -> ZLM 解 RTP -> PS 解复用 -> H264/G711 -> ZLM 内部流
```

区别在于：

| 项目 | 直播 | 回放 |
|---|---|---|
| 控制指令 | 实时音视频请求 | 远程录像回放请求 |
| 数据来源 | 摄像头实时编码 | 终端本地录像 |
| 流 ID | `{sim}_{channel}_live` | `{sim}_{channel}_replay_{taskId}` |
| 暂停 | 一般不支持 | 支持 |
| 拖动 | 不支持 | 支持 |
| 倍速 | 不支持 | 支持 |
| ZLM 职责 | 接收实时流 | 接收历史流 |
| 业务平台职责 | 下发直播控制 | 下发回放和回放控制 |

### 9.2 步骤 1：业务平台新增回放任务

建议表结构：

```sql
CREATE TABLE video_replay_task (
    id BIGINT PRIMARY KEY,
    task_id VARCHAR(64),
    device_id VARCHAR(32),
    channel INT,
    start_time DATETIME,
    end_time DATETIME,
    status INT,
    stream_id VARCHAR(128),
    created_at DATETIME,
    updated_at DATETIME
);
```

状态建议：

```text
0 待开始
1 推流中
2 暂停中
3 已结束
4 失败
```

### 9.3 步骤 2：回放流 ID 设计

不要和直播流混用。

建议：

```text
{sim}_{channel}_replay_{taskId}
```

例如：

```text
013912345678_1_replay_20260508123000123
```

完整流：

```text
app = jt1078
stream = 013912345678_1_replay_20260508123000123
```

### 9.4 步骤 3：下发回放请求

流程：

```text
前端选择时间段
    |
    v
业务平台创建 replayTask
    |
    v
业务平台下发 1078 回放请求
    |
    v
终端推历史 RTP 到 ZLM
    |
    v
ZLM 根据 SIM + 通道 + 任务上下文创建 replay stream
```

接口：

```http
POST /vehicle/video/replay/start
```

请求：

```json
{
  "deviceId": "013912345678",
  "channel": 1,
  "startTime": "2026-05-08 10:00:00",
  "endTime": "2026-05-08 10:10:00",
  "speed": 1
}
```

返回：

```json
{
  "taskId": "20260508123000123",
  "streamId": "013912345678_1_replay_20260508123000123",
  "flvUrl": "http://127.0.0.1:8080/jt1078/013912345678_1_replay_20260508123000123.live.flv"
}
```

### 9.5 步骤 4：ZLM 新增 ReplayManager

```cpp
class JT1078ReplayManager {
public:
    void createTask(const std::string& taskId,
                    const std::string& sim,
                    int channel,
                    const std::string& streamId);

    std::string findStreamId(const std::string& sim, int channel);

    void pause(const std::string& taskId);
    void resume(const std::string& taskId);
    void stop(const std::string& taskId);
    void timeoutCheck();

private:
    std::unordered_map<std::string, ReplayTask> tasks_;
};
```

注意：

```text
ZLM 不负责真正的拖动/暂停/倍速。
这些动作还是业务平台下发给终端。
ZLM 只维护 replay 流生命周期。
```

### 9.6 步骤 5：回放控制接口

业务平台接口：

```http
POST /vehicle/video/replay/pause
POST /vehicle/video/replay/resume
POST /vehicle/video/replay/seek
POST /vehicle/video/replay/speed
POST /vehicle/video/replay/stop
```

对应终端控制：

```text
暂停回放
继续回放
拖动回放
倍速回放
停止回放
```

ZLM 只配合：

```text
关闭流
更新流状态
超时释放 session
```

---

## 10. 第三阶段：音频接入

语音对讲前，先把终端上行音频解析出来。

### 10.1 步骤 1：PS Demux 支持音频 PES

识别：

```text
0x000001C0 音频流
```

提取音频 payload。

第一版只做：

```text
G711A
8000 Hz
单声道
20ms 一帧
```

### 10.2 步骤 2：接入 ZLM AudioTrack

音频输出链路：

```text
JT1078 RTP -> PS -> PES -> G711A -> ZLM AudioTrack
```

这样前端播放直播或回放时可以听到车内声音。

---

## 11. 第四阶段：JT1078 语音对讲

### 11.1 对讲链路

```text
浏览器麦克风
    |
    | WebRTC / WebSocket
    v
ZLM TalkSession
    |
    | PCM / Opus -> G711A
    v
JT1078AudioRtpPacker
    |
    | JT1078 RTP Audio
    v
终端

终端麦克风
    |
    | JT1078 RTP Audio
    v
ZLM
    |
    | G711A -> WebRTC / WS
    v
浏览器
```

### 11.2 第一步建议做半双工

第一版不要直接做复杂全双工。

建议：

```text
按住说话：平台 -> 终端
松开说话：终端 -> 平台
```

优点：

```text
实现简单
回声问题少
终端兼容性更好
```

### 11.3 新增 TalkSession

```cpp
class JT1078TalkSession {
public:
    void start();
    void stop();

    void inputFromBrowser(const uint8_t* pcm, size_t len);
    void inputFromTerminal(const JT1078RtpPacket& pkt);

private:
    std::string sim_;
    int channel_ = 0;
    std::string talkId_;

    JT1078AudioEncoder audioEncoder_;
    JT1078AudioRtpPacker rtpPacker_;
};
```

### 11.4 浏览器音频输入方式

#### 方案 A：WebSocket PCM，第一版推荐

```text
浏览器采集麦克风 PCM
    |
    v
WebSocket 上传 PCM
    |
    v
ZLM / TalkGateway 转 G711A
    |
    v
JT1078 RTP 发给终端
```

优点：

```text
实现可控
调试简单
不强依赖 WebRTC
适合第一版跑通
```

#### 方案 B：WebRTC，生产版推荐

```text
浏览器 WebRTC
    |
    v
ZLM WebRTC
    |
    v
Opus / PCM
    |
    v
JT1078TalkAdapter 转 G711A
    |
    v
JT1078 RTP Audio
    |
    v
终端
```

优点：

```text
浏览器体验好
延迟低
音频采集标准
适合正式产品
```

### 11.5 音频编码建议

第一版统一：

```text
PCM 16bit 8kHz 单声道
转 G711A
20ms 一帧
每帧 160 字节 G711A
```

封装为 JT1078 RTP 音频包：

```text
RTP Header
+
JT1078 Header
+
G711A payload
```

### 11.6 对讲 API 设计

业务平台：

```http
POST /vehicle/talk/start
POST /vehicle/talk/stop
POST /vehicle/talk/ptt/start
POST /vehicle/talk/ptt/stop
```

ZLM：

```http
POST /index/api/jt1078/startTalk
POST /index/api/jt1078/stopTalk
GET  /index/api/jt1078/getTalkList
```

返回示例：

```json
{
  "code": 0,
  "talkId": "talk_013912345678_1_20260508124500",
  "uploadUrl": "ws://127.0.0.1:8080/jt1078/talk/upload/talk_013912345678_1_20260508124500",
  "playUrl": "ws://127.0.0.1:8080/jt1078/talk/play/talk_013912345678_1_20260508124500"
}
```

---

## 12. ZLM 新增 API

### 12.1 查询 JT1078 流

```http
GET /index/api/jt1078/getStreamList
```

返回：

```json
{
  "code": 0,
  "data": [
    {
      "sim": "013912345678",
      "channel": 1,
      "type": "live",
      "stream": "013912345678_1_live",
      "videoCodec": "H264",
      "audioCodec": "G711A",
      "online": true
    }
  ]
}
```

### 12.2 关闭 JT1078 流

```http
POST /index/api/jt1078/closeStream
```

请求：

```json
{
  "sim": "013912345678",
  "channel": 1,
  "streamId": "013912345678_1_live"
}
```

### 12.3 创建回放任务

```http
POST /index/api/jt1078/createReplayTask
```

请求：

```json
{
  "taskId": "20260508123000123",
  "sim": "013912345678",
  "channel": 1,
  "streamId": "013912345678_1_replay_20260508123000123"
}
```

### 12.4 关闭回放任务

```http
POST /index/api/jt1078/closeReplayTask
```

### 12.5 开始对讲

```http
POST /index/api/jt1078/startTalk
```

### 12.6 停止对讲

```http
POST /index/api/jt1078/stopTalk
```

---

## 13. 业务平台接口设计

### 13.1 直播开始

```text
/video/live/start
    |
    | 1. 检查车辆在线
    | 2. 生成 streamId
    | 3. 调用 ZLM 创建或准备流
    | 4. 下发 JT808 0x9101
    | 5. 返回播放 URL
```

### 13.2 直播停止

```text
/video/live/stop
    |
    | 1. 下发停止实时音视频
    | 2. 调用 ZLM closeStream
    | 3. 更新状态
```

### 13.3 回放开始

```text
/video/replay/start
    |
    | 1. 创建 replayTask
    | 2. 调用 ZLM createReplayTask
    | 3. 下发回放请求
    | 4. 返回 replay 播放 URL
```

### 13.4 回放控制

```http
POST /video/replay/pause
POST /video/replay/resume
POST /video/replay/seek
POST /video/replay/speed
POST /video/replay/stop
```

这些接口主要是下发终端控制指令，ZLM 不直接做 seek。

### 13.5 对讲开始

```text
/talk/start
    |
    | 1. 检查车辆在线
    | 2. 检查是否已有对讲
    | 3. 下发对讲控制
    | 4. 调用 ZLM startTalk
    | 5. 返回 uploadUrl/playUrl
```

---

## 14. 开发顺序

### 第 1 步：ZLM 能收 1078 TCP

验收：

```text
终端或模拟器连接 1078 端口
ZLM 打印连接日志
ZLM 打印收到数据长度
```

### 第 2 步：解析 JT1078 RTP 头

验收日志：

```text
sim=013912345678
channel=1
dataType=0x00
packetType=0x00
seq=1024
timestamp=12345678
payloadSize=1024
```

### 第 3 步：RTP 分包重组

验收：

```text
原子包正常输出
分包能合成完整 PS
丢包能丢弃当前帧
不会内存无限增长
```

### 第 4 步：PS 解复用提取 H264

验收：

```text
能识别 0x000001BA
能识别 0x000001E0
能提取 H264 NALU
能保存成 .h264 文件
ffplay 可以播放
```

### 第 5 步：接入 ZLM MediaSource

验收：

```text
ZLM 出现 jt1078/xxx_live 流
/index/api/getMediaList 能看到流
HTTP-FLV 能播放
```

### 第 6 步：接入业务平台 0x9101

验收：

```text
前端点播放
平台下发 0x9101
终端推流
ZLM 生成流
浏览器播放
```

### 第 7 步：支持回放任务

验收：

```text
前端选择时间段
平台下发回放指令
终端推历史 RTP
ZLM 生成 replay 流
浏览器播放历史视频
```

### 第 8 步：支持 G711A 音频

验收：

```text
直播画面有声音
回放有声音
音视频时间戳基本同步
```

### 第 9 步：支持语音对讲

验收：

```text
浏览器麦克风 -> 终端能听到
终端麦克风 -> 浏览器能听到
开始/停止对讲状态正确
断线能释放资源
```

---

## 15. 关键风险点

### 15.1 终端封包不标准

不同厂家可能差异很大：

```text
SIM BCD 编码不同
RTP header extension 不同
PS 封装不标准
H264 是否带 start code 不一致
时间戳单位不同
音频格式不同
```

建议：

```text
解析器要宽松
日志要详细
保留原始包 dump 功能
关键字段增加异常保护
```

### 15.2 PS 解复用是核心难点

第一版不要追求完整支持所有情况，建议先做：

```text
H264
G711A
常见 PS/PES
```

### 15.3 语音对讲不要第一版做全双工

第一版建议：

```text
半双工
G711A
8kHz
20ms
WebSocket PCM 输入
```

跑通后再升级 WebRTC。

### 15.4 回放控制不是 ZLM 的核心职责

暂停、继续、拖动、倍速，本质是：

```text
业务平台 -> JT808/1078 控制指令 -> 终端
```

ZLM 只负责接收终端推过来的回放 RTP 流。

---

## 16. 推荐排期

| 阶段 | 内容 | 预计 |
|---|---:|---:|
| 1 | JT1078 TCP 接入 | 2-3 天 |
| 2 | RTP 解析和分包重组 | 4-6 天 |
| 3 | PS 解复用 H264 | 5-8 天 |
| 4 | 接入 ZLM MediaSource | 4-6 天 |
| 5 | 直播联调 | 3-5 天 |
| 6 | 回放任务和回放控制 | 5-8 天 |
| 7 | G711A 音频接入 | 3-5 天 |
| 8 | 语音对讲第一版 | 8-15 天 |
| 9 | 兼容性和压测 | 持续 |

比较现实的周期：

```text
直播第一版：2-3 周
直播 + 回放：4-5 周
直播 + 回放 + 对讲：6-8 周
生产可用：2-3 个月
```

---

## 17. 最终落地路线

建议按以下顺序开干：

```text
1. 新建 src/JT1078 目录
2. 接入 CMake 编译
3. 启动 JT1078TcpServer 监听 1078
4. JT1078Session 收 TCP 数据
5. JT1078RtpDecoder 解析 RTP
6. JT1078FrameAssembler 重组 payload
7. JT1078PsDemuxer 提取 H264
8. 先落盘 xxx.h264，用 ffplay 验证
9. 接入 ZLM MediaSource，输出 HTTP-FLV
10. Spring Boot 808 网关下发 0x9101
11. 做 replayTask 区分回放流
12. 增加 G711A 音频
13. 增加 TalkSession 做语音对讲
```

---

## 18. 总结

最终方案可以概括为：

```text
ZLM 不直接理解 JT808 控制信令。
ZLM 只负责 JT1078 媒体接入和分发。
808 网关负责下发直播、回放、对讲指令。
业务平台负责设备、权限、任务、状态。
ZLM 新增 JT1078 模块，把 RTP/PS/G711/H264 转成内部 MediaSource。
前端统一播放 ZLM 输出的 HTTP-FLV / WS-FLV / WebRTC。
```

第一阶段最重要的验收目标：

```text
让 ZLM 收到 JT1078 RTP，并成功吐出 H264。
然后接入 ZLM 内部 MediaSource。
最后再做回放任务和语音对讲。
```
