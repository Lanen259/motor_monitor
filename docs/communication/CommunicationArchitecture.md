# 通信架构设计 (Communication Architecture)

> 版本: 1.0 | 状态: 设计阶段 | 作者: 系统架构组

---

## 1. 目标 (Goal)

构建一套面向电机监控系统的**异步、可扩展、多协议**通信基础设施，实现以下核心目标：

- **零阻塞**：所有 I/O 操作异步化，UI 线程永不阻塞于通信操作。
- **协议无关**：上层业务逻辑不感知底层传输介质（串口、TCP、SPI、CAN 等）。
- **多设备并发**：同时管理与多个下位机设备的连接，支持动态发现与热插拔。
- **高可靠**：命令超时重试、CRC 校验、帧同步、丢包检测，确保工业现场数据完整性。
- **实时数据流**：支持高频采样数据（如 10kHz 电流波形）的实时接收与解析，背压可控。
- **固件升级**：支持固件升级专用模式，独占传输通道，暂停常规数据流。

---

## 2. 设计原则 (Design Principles)

| 原则 | 说明 |
|------|------|
| **分层解耦** | 严格四层架构：Transport → Protocol → Command → Stream，每层只依赖下层接口 |
| **异步优先** | 全部使用 Promise/Future + Signal/Slot 模式，禁止 `wait()` / `exec()` 同步阻塞 |
| **接口隔离** | 传输层抽象为 `ITransport` 纯虚接口，新协议只需实现该接口 |
| **零拷贝** | 数据路径中使用 `QByteArray` 隐式共享与 `std::span`，避免不必要拷贝 |
| **可测试性** | 每层均可独立单元测试，通过 Mock Transport 注入测试数据 |
| **资源 RAII** | 传输连接、定时器、缓冲区均由 RAII 管理，析构时自动释放 |
| **背压感知** | 当消费端处理速度落后于生产端时，自动触发流控（XON/XOFF 或 RTS/CTS） |

---

## 3. 类/模块关系 (Class/Module Relationships)

### 3.1 四层架构总览

```
┌──────────────────────────────────────────────────────┐
│                    Stream Layer                       │
│  StreamParser  /  DataPublisher  /  WaveformBuffer   │
├──────────────────────────────────────────────────────┤
│                   Command Layer                       │
│  CommandQueue  /  CommandDispatcher  /  PendingPool  │
├──────────────────────────────────────────────────────┤
│                   Protocol Layer                      │
│  FrameCodec  /  COBSEncoder  /  CRC16  /  Fragmenter │
├──────────────────────────────────────────────────────┤
│                  Transport Layer                      │
│  ITransport  /  SerialTransport  /  TcpTransport     │
│  TransportFactory  /  TransportManager               │
└──────────────────────────────────────────────────────┘
```

### 3.2 核心类 UML

```
┌─────────────────────┐        ┌──────────────────────────┐
│  CommunicationManager│──────▶│     TransportManager      │
│   (Facade)          │        │  - transports: QHash<id>  │
│  - cmdQueue         │        │  - register(ITransport*)  │
│  - streamParser     │        │  - connectTo(id, params)  │
│  - transportMgr     │        │  - disconnect(id)         │
│  - fwUpdater        │        │  - availableDevices()     │
└─────────────────────┘        └──────────┬───────────────┘
                                          │ 0..*
                                 ┌────────▼──────────┐
                                 │    ITransport      │
                                 │  + open(DeviceInfo)│
                                 │  + close()         │
                                 │  + send(QByteArray)│
                                 │  + isOpen()        │
                                 │  signal:           │
                                 │   dataReceived()   │
                                 │   errorOccurred()  │
                                 │   connectionState()│
                                 └──┬──────┬──────┬──┘
                           ┌────────▼┐ ┌───▼──┐ ┌──▼────────┐
                           │ Serial  │ │ TCP  │ │  SPI       │
                           │Transport│ │Trans.│ │ Transport  │
                           └─────────┘ └──────┘ └───────────┘

┌──────────────────────┐        ┌──────────────────────┐
│    CommandQueue      │        │    FrameCodec        │
│  - enqueue(cmd)      │        │  State: enum         │
│  - dequeue()         │        │  - feed(byte)        │
│  - pendingPool       │        │  signal:             │
│  - priorityHeap      │        │   frameReady(Frame)  │
│  - retryTimer        │        │   frameError(Error)  │
│  signal:             │        └──────────────────────┘
│   responseReady()    │
│   commandTimeout()   │
└──────────────────────┘

┌──────────────────────┐        ┌──────────────────────┐
│    StreamParser      │        │    DataPublisher     │
│  - parseFrame(Frame) │        │  - subscribe(key)    │
│  - variableRegistry  │        │  - unsubscribe(key)  │
│  signal:             │        │  signal:             │
│   dataPoint(key,val) │        │   newData(DataPoint) │
│   waveform(data)     │        └──────────────────────┘
└──────────────────────┘
```

### 3.3 模块依赖关系

```
CommunicationManager (Facade, 聚合所有子模块)
    ├── TransportManager
    │       └── ITransport (多态, 插件式)
    ├── CommandQueue
    │       ├── FrameCodec (协议编码/解码)
    │       │   ├── COBSEncoder
    │       │   └── CRC16
    │       └── Fragmenter (大包分片)
    └── StreamParser
            └── DataPublisher
                    └── DataBus (全局数据总线, 观察者模式)
```

---

## 4. 数据流 (Data Flow)

### 4.1 命令发送流程 (上行)

```
UI/Controller
    │
    ▼
CommandQueue.enqueue(Command)
    │
    ├─ 分配 seqId (自增, 单调)
    ├─ 插入 PendingPool (seqId → Promise)
    ├─ 按优先级入堆
    │
    ▼
CommandDispatcher (消费队列)
    │
    ├─ FrameCodec.encode(Command) → Frame
    │   ├─ 大包? → Fragmenter.split()
    │   └─ COBS + CRC16
    │
    ▼
TransportManager.getTransport(deviceId)
    │
    ▼
ITransport.send(rawBytes)
    │
    ▼
物理层发送
```

### 4.2 命令响应流程 (下行)

```
物理层接收
    │
    ▼
ITransport.dataReceived(QByteArray)
    │
    ▼
FrameCodec.feed(byte) [状态机]
    │
    ├─ 帧完整 → frameReady(Frame)
    │
    ▼
CommandDispatcher.dispatch(Frame)
    │
    ├─ CMD范围 (0x0000-0x7FFF) → PendingPool.resolve(seqId, Frame)
    │       └─ Promise.finished() → Signal → UI
    │
    └─ STREAM范围 (0x8000-0xFFFF) → StreamParser.parse(Frame)
            └─ DataPublisher.publish(DataPoint) → DataBus → Curve/Sink
```

### 4.3 实时数据流路径

```
下位机 (10kHz ADC)
    │
    ▼
ITransport::dataReceived
    │
    ▼
FrameCodec → Frame (CMD=0x8001, 流数据)
    │
    ▼
StreamParser.parse()
    │
    ├─ 解析 timestamp + variable[] 
    ├─ 写入 RingBuffer(DataBuffer)
    │
    ▼
DataPublisher (Signal 通知)
    │
    ▼
CurveEngine::onNewData() → LTTB 降采样 → GPU 渲染
```

---

## 5. API接口规划 (API Interface Planning)

### 5.1 ITransport 接口

```cpp
// itransport.h
class ITransport : public QObject {
    Q_OBJECT
public:
    virtual ~ITransport() = default;

    // 生命周期
    virtual bool open(const DeviceInfo& info) = 0;
    virtual void close() = 0;
    virtual bool isOpen() const = 0;

    // 数据收发
    virtual qint64 send(const QByteArray& data) = 0;
    virtual qint64 bytesAvailable() const = 0;

    // 传输元信息
    virtual QString transportType() const = 0;
    virtual DeviceInfo deviceInfo() const = 0;

    // 流控
    virtual void setFlowControl(FlowControl mode) = 0;
    virtual bool isFlowPaused() const = 0;

    // 独占模式（固件升级）
    virtual bool acquireExclusive() = 0;
    virtual void releaseExclusive() = 0;

signals:
    void dataReceived(const QByteArray& data);
    void errorOccurred(TransportError error, const QString& message);
    void connectionStateChanged(ConnectionState state);
    void flowStateChanged(bool paused);
};
```

### 5.2 TransportManager 接口

```cpp
// transportmanager.h
class TransportManager : public QObject {
    Q_OBJECT
public:
    // 注册/注销传输插件
    void registerTransport(const QString& type, 
                           std::function<ITransport*()> factory);
    void unregisterTransport(const QString& type);

    // 设备管理
    QFuture<bool> connectTo(const QString& deviceId, 
                            const DeviceInfo& info);
    QFuture<void> disconnect(const QString& deviceId);
    QStringList connectedDevices() const;
    QList<DeviceInfo> discoverDevices();

    // 获取传输实例
    ITransport* transport(const QString& deviceId);

    // 广播
    QFuture<void> broadcast(const QByteArray& data);

    // 独占模式
    QFuture<bool> setExclusiveMode(const QString& deviceId, bool exclusive);

signals:
    void deviceConnected(const QString& deviceId);
    void deviceDisconnected(const QString& deviceId);
    void deviceError(const QString& deviceId, const QString& error);
};
```

### 5.3 CommandQueue 接口

```cpp
// commandqueue.h
class CommandQueue : public QObject {
    Q_OBJECT
public:
    enum Priority { Low = 0, Normal = 1, High = 2, Critical = 3 };

    struct Command {
        uint16_t cmdCode;
        QByteArray payload;
        Priority priority;
        std::chrono::milliseconds timeout;
        int maxRetries;
    };

    // 发送命令 (返回 Future)
    QFuture<Frame> send(const QString& deviceId, const Command& cmd);

    // 取消等待中的命令
    void cancel(uint16_t seqId);

    // 队列状态
    int pendingCount(const QString& deviceId) const;
    int queueSize(const QString& deviceId) const;

signals:
    void responseReady(const QString& deviceId, uint16_t seqId, const Frame& frame);
    void commandTimeout(const QString& deviceId, uint16_t seqId);
    void commandRetry(const QString& deviceId, uint16_t seqId, int attempt);
    void allRetriesExhausted(const QString& deviceId, uint16_t seqId);
};
```

### 5.4 FrameCodec 接口

```cpp
// framecodec.h
class FrameCodec : public QObject {
    Q_OBJECT
public:
    enum State { Idle, STX1, STX2, LenLow, LenHigh, Payload, CrcLow, CrcHigh, ETX1, ETX2 };

    // 喂入单个字节，状态机驱动
    void feed(uint8_t byte);
    void feed(const QByteArray& data);

    // 编码帧
    static QByteArray encode(uint16_t cmd, const QByteArray& payload);
    static QByteArray encodeFrame(const Frame& frame);

    // 重置状态机
    void reset();

    // 调试
    State currentState() const;

signals:
    void frameReady(const Frame& frame);
    void frameError(FrameError error, const QString& detail);
    void stateChanged(State oldState, State newState);
};
```

### 5.5 StreamParser 接口

```cpp
// streamparser.h
class StreamParser : public QObject {
    Q_OBJECT
public:
    // 注册变量定义
    void registerVariable(uint16_t id, DataType type, const QString& name, 
                          const QString& unit, float scale = 1.0f, float offset = 0.0f);

    // 解析流数据帧
    void parseStreamFrame(const Frame& frame);

    // 波形数据解析
    WaveformData parseWaveform(const Frame& frame);

signals:
    void dataPointReady(const QString& deviceId, uint64_t timestamp, 
                        const QVector<VariableValue>& values);
    void waveformReady(const QString& deviceId, const WaveformData& waveform);
    void parseError(const QString& deviceId, const QString& error);
};
```

### 5.6 CommunicationManager 接口 (Facade)

```cpp
// communicationmanager.h
class CommunicationManager : public QObject {
    Q_OBJECT
public:
    static CommunicationManager* instance();

    // 设备连接
    QFuture<bool> connectDevice(const DeviceInfo& info);
    QFuture<void> disconnectDevice(const QString& deviceId);

    // 发送命令 (高层封装)
    QFuture<Frame> sendCommand(const QString& deviceId, uint16_t cmdCode,
                               const QByteArray& payload,
                               CommandQueue::Priority priority = CommandQueue::Normal);

    // 流数据订阅
    void subscribeStream(const QString& deviceId, uint16_t variableId);
    void unsubscribeStream(const QString& deviceId, uint16_t variableId);

    // 固件升级
    FirmwareUpdater* firmwareUpdater(const QString& deviceId);

    // 状态查询
    ConnectionState connectionState(const QString& deviceId) const;
    CommunicationStats stats(const QString& deviceId) const;

signals:
    void deviceStateChanged(const QString& deviceId, ConnectionState state);
    void streamDataReceived(const QString& deviceId, const VariableValue& value);
    void globalError(const QString& deviceId, const QString& error);
};
```

---

## 6. 关键设计决策与实现细节

### 6.1 异步设计模式

**禁止同步等待**：
```cpp
// ❌ 错误：阻塞 UI 线程
auto response = sendCommandSync(...);  // wait() 阻塞

// ✅ 正确：Promise + 回调
sendCommand(...)
    .then([](const Frame& resp) { /* 处理响应 */ })
    .onTimeout([](uint16_t seqId) { /* 超时处理 */ });
```

**Promise 实现建议**：使用 `QFuture` + `QFutureInterface` 或 `QPromise`（Qt6），配合 `QtConcurrent`。

### 6.2 命令队列与 seqId 匹配

```
seqId: 16-bit 自增 (0x0001-0xFFFF, 回绕)
PendingPool: QHash<uint16_t, PendingCommand>
    - 每个 pending 命令包含: cmd, seqId, timeout, retryCount, promise
    - 收到响应帧时，通过 seqId 查找 PendingPool
    - 超时定时器: QTimer (100ms interval), 遍历检查超时
```

### 6.3 重连机制

```
指数退避: delay = min(baseDelay * 2^attempt, maxDelay)
- baseDelay: 500ms
- maxDelay: 30s
- 最大重试: 10次后通知用户，持续后台尝试
- jitter: ±20% 随机偏移，避免雷击效应
```

### 6.4 固件升级独占模式

```
进入升级模式:
1. CommunicationManager::enterFirmwareMode(deviceId)
2. → StreamParser 暂停该设备的数据流
3. → TransportManager::setExclusiveMode(deviceId, true)
4. → 其他命令排队，仅 FirmwareUpdater 可发送
5. → 升级完成后 releaseExclusive → 恢复流数据
```

### 6.5 添加新传输协议 (3步)

```
Step 1: 实现 ITransport 接口
  class SpiTransport : public ITransport { ... };

Step 2: 注册到 TransportFactory
  TransportFactory::instance()->registerTransport("SPI", 
      []() { return new SpiTransport(); });

Step 3: 配置设备信息
  DeviceInfo info;
  info.transportType = "SPI";
  info.params["spiDevice"] = "/dev/spidev0.0";
  info.params["speed"] = "10000000";
  CommunicationManager::instance()->connectDevice(info);
```

---

## 7. 后续实现注意事项 (Implementation Notes)

| 类别 | 注意事项 |
|------|----------|
| **线程安全** | `CommandQueue` 的 `PendingPool` 必须使用 `QMutex` 保护；`FrameCodec::feed()` 非线程安全，需在数据接收线程中串行调用 |
| **内存管理** | `ITransport` 子类由 `TransportManager` 持有所有权；`CommunicationManager` 为单例，使用 `Q_GLOBAL_STATIC` 确保线程安全初始化 |
| **性能** | 串口接收使用 `QSerialPort::readyRead` + `readAll()` 批量读取，避免逐字节触发；TCP 使用 `QTcpSocket` 的异步 API |
| **错误处理** | 每层错误分类（TransportError / ProtocolError / CommandError），通过 signal 向上传播，UI 层统一展示 |
| **日志** | 使用 `qCDebug` 分类日志：`log.comm.transport` / `log.comm.protocol` / `log.comm.command` / `log.comm.stream` |
| **单元测试** | 为 `FrameCodec` 编写参数化测试（合法帧、边界条件、恶意帧）；为 `CommandQueue` 编写超时/重试/乱序测试 |
| **跨平台** | 串口实现使用 `QSerialPort`（Qt 已封装）；避免平台特定 API；路径使用 `QDir::toNativeSeparators()` |
| **配置** | 传输参数（波特率、超时、重试次数）从 `QSettings` 读取，支持运行时热更新 |
| **固件升级** | 升级数据包格式与普通命令不同，需独立分片逻辑（每片 256B），升级完成后校验整体 CRC32 |
| **向后兼容** | 协议版本号在握手帧中交换，旧版本兼容策略在 `FrameCodec` 中处理 |

---

## 附录 A: 通信统计结构

```cpp
struct CommunicationStats {
    QString deviceId;
    qint64 bytesSent;
    qint64 bytesReceived;
    int framesSent;
    int framesReceived;
    int framesDropped;       // CRC 校验失败
    int frameErrors;         // 格式错误
    int timeoutCount;
    int retryCount;
    double avgRttMs;         // 平均往返时间
    QDateTime connectedSince;
    QDateTime lastActivity;
};
```

## 附录 B: 错误码枚举

```cpp
enum class TransportError {
    None = 0,
    OpenFailed,
    PermissionDenied,
    DeviceNotFound,
    ReadError,
    WriteError,
    ConnectionLost,
    Timeout,
    BufferOverflow,
};

enum class FrameError {
    None = 0,
    InvalidSTX,
    InvalidETX,
    CRCMismatch,
    LengthMismatch,
    BufferOverflow,
    UnknownCommand,
    COBSDecodeError,
};
```