# DataFlow — 数据流架构

> **文档版本**: v1.0  
> **父文档**: [SystemArchitecture.md](SystemArchitecture.md)  

---

## 目标

定义 Motor Studio 的端到端数据流架构，包括上行数据流（MCU → UI）、下行数据流（UI → MCU）、DataBus 发布/订阅模型、Topic 系统、RingBuffer 设计以及性能目标。确保数据在系统中高效、可靠、低延迟地传递。

---

## 设计原则

| 原则 | 说明 |
|------|------|
| **单向数据流** | 上行与下行路径严格分离，避免循环依赖 |
| **广播语义** | DataBus 发布数据时广播给所有订阅者，而非消费式（一个消息仅一个消费者） |
| **零拷贝** | 关键路径上使用 `std::span` / 共享内存，避免数据拷贝 |
| **整数 Topic ID** | 使用整数 ID 而非字符串，配合数组索引实现 O(1) 查找 |
| **背压感知** | RingBuffer 溢出时丢弃最旧数据并计数，不阻塞生产者 |
| **可观测性** | 每个环节导出吞吐量、延迟、错误计数指标 |

---

## 上行数据流（MCU → UI）

### 流程图

```
┌──────┐    raw bytes     ┌──────────────┐    byte buffer     ┌────────────────┐
│ MCU  │ ───────────────► │  Transport   │ ────────────────►  │   Protocol     │
│      │   (UART/CAN)     │  (I/O Thread)│                    │   (parse)      │
└──────┘                  └──────────────┘                    └───────┬────────┘
                                                                     │
                                                    parsed DataPoint  │
                                                                     ▼
                                                          ┌─────────────────┐
                                                          │    DataBus      │
                                                          │  ┌───────────┐  │
                                                          │  │ RingBuffer│  │
                                                          │  │ (per topic)│  │
                                                          │  └───────────┘  │
                                                          │  ┌───────────┐  │
                                                          │  │ DataCache │  │
                                                          │  └───────────┘  │
                                                          └───────┬─────────┘
                                                                  │
                                      ┌───────────────────────────┼───────────────────────────┐
                                      │                           │                           │
                                      ▼                           ▼                           ▼
                            ┌─────────────────┐       ┌─────────────────┐       ┌─────────────────┐
                            │ DataMonitor     │       │ CurveEngine     │       │  Other          │
                            │ Service         │       │ Service         │       │  Subscribers    │
                            └───────┬─────────┘       └───────┬─────────┘       └───────┬─────────┘
                                    │                         │                         │
                                    ▼                         ▼                         ▼
                            ┌─────────────────┐       ┌─────────────────┐       ┌─────────────────┐
                            │ Dashboard       │       │ Scope           │       │  Custom         │
                            │ ViewModel       │       │ ViewModel       │       │  ViewModel      │
                            └───────┬─────────┘       └───────┬─────────┘       └───────┬─────────┘
                                    │                         │                         │
                                    ▼                         ▼                         ▼
                            ┌─────────────────┐       ┌─────────────────┐       ┌─────────────────┐
                            │ Dashboard       │       │ Scope           │       │  Custom         │
                            │ View (QML)      │       │ View (QML)      │       │  View (QML)     │
                            └─────────────────┘       └─────────────────┘       └─────────────────┘
```

### 各阶段详细说明

#### 阶段 1: Transport 层（原始字节接收）

```
Transport Thread (Comm Thread)
│
├── SerialTransport::readLoop()
│   ├── 非阻塞读取串口缓冲区
│   ├── 累积到内部 byte buffer
│   └── 触发 onBytesReceived 回调
│
└── 数据通过 Lock-free SPSC Queue → DataBus Thread
```

**性能目标**：
- 单次读取延迟：< 100μs（从串口缓冲区到回调触发）
- 缓冲区大小：4KB（可配置）

#### 阶段 2: Protocol 层（帧解析）

```
DataBus Thread
│
├── ProtocolManager::onBytesReceived(buffer)
│   └── ModbusProtocol::parse(buffer)
│       ├── 帧边界检测（Modbus RTU: 3.5 字符间隔）
│       ├── CRC 校验
│       ├── 功能码解析（03/06/10/16）
│       ├── 数据提取：uint16 → float/int32（按寄存器映射表）
│       └── 构造 DataPoint 结构体
│
└── DataPoint 推入 DataBus
```

**DataPoint 结构体**：
```cpp
struct DataPoint {
    TopicId     topic_id;       // 整数 Topic ID
    Timestamp   timestamp;      // 数据产生时间戳（μs 精度）
    Value       value;          // 变体值（variant）
    Quality     quality;        // 数据质量标记
    uint64_t    sequence;       // 全局递增序号

    enum class Quality {
        Good,                   // 正常数据
        Stale,                  // 超时未更新
        Uncertain,              // 不确定（如 CRC 校验失败后重试成功）
        Bad                     // 无效数据
    };
};

using Value = std::variant<
    int32_t, uint32_t,
    float, double,
    bool,
    std::string                  // 预留，不建议高频使用
>;

using Timestamp = std::chrono::microseconds;  // 自 epoch 的微秒计数
```

#### 阶段 3: DataBus 分发

```
DataBus::publish(DataPoint)
│
├── 1. 写入 RingBuffer（对应 Topic）
│   └── ringbuffer_[topic_id].push(data_point);
│
├── 2. 更新 DataCache（最新值）
│   └── cache_[topic_id] = data_point;
│
├── 3. 遍历订阅者列表
│   └── for (auto& sub : subscribers_[topic_id]) {
│           sub.callback(data_point);  // 广播语义
│       }
│
└── 4. 更新统计计数器
    └── stats_.published_count++;
```

**广播语义 vs 消费语义**：

```
消费语义（错误）:
    Producer → [Queue] → Consumer_1 (弹出，Consumer_2 看不到)

广播语义（正确）:
    Producer → DataBus → Consumer_1 (收到)
                       → Consumer_2 (收到)
                       → Consumer_3 (收到)
    所有订阅者同时收到同一份数据
```

#### 阶段 4: Service 层处理

```
DataMonitorService::onDataPoint(DataPoint dp)
│
├── 检查阈值
│   └── if (dp.value > max_threshold) → emit alarm
│
├── 计算衍生指标
│   └── moving_average.update(dp.value)
│
└── 通知 ViewModel（通过 Qt Signal）
    └── emit dataUpdated(dp);

CurveEngineService::onDataPoint(DataPoint dp)
│
├── 追加到波形缓冲区
│   └── channel_buffer_[dp.topic_id].append(dp);
│
├── 触发渲染准备（降采样）
│   └── if (buffer_size > render_threshold) → prepareRenderData()
│
└── 通知 ViewModel
    └── emit newCurveData(render_data);
```

#### 阶段 5: UI 层渲染

```
ScopeViewModel::onNewCurveData(CurveData data)
│
├── 更新 Q_PROPERTY
│   └── setCurvePoints(data.points);
│
└── QML Binding 自动触发重绘
    └── Canvas { onPaint: { drawCurve(viewModel.curvePoints) } }
```

---

## 下行数据流（UI → MCU）

### 流程图

```
┌─────────────────┐
│ Config View     │  用户输入参数值
│ (QML)           │
└───────┬─────────┘
        │
        ▼
┌─────────────────┐
│ Config          │  参数校验、单位转换
│ ViewModel       │
└───────┬─────────┘
        │
        ▼
┌─────────────────┐
│ ParamManager    │  参数写入逻辑、权限检查
│ Service         │
└───────┬─────────┘
        │
        ▼
┌─────────────────┐
│  CommandQueue   │  优先级排序
│  ┌───────────┐  │
│  │ Priority 0│  │  ← 紧急停止
│  │ Priority 1│  │  ← 参数写入
│  │ Priority 2│  │  ← 查询读取
│  └───────────┘  │
└───────┬─────────┘
        │
        ▼
┌─────────────────┐
│   Protocol      │  命令编码（Modbus 功能码封装）
│   (encode)      │
└───────┬─────────┘
        │
        ▼
┌─────────────────┐
│   Transport     │  物理发送
│   (I/O Thread)  │
└───────┬─────────┘
        │
        ▼
┌──────┐
│ MCU  │
└──────┘
```

### 各阶段详细说明

#### 阶段 1: UI 输入

```cpp
// ConfigView.qml
TextField {
    id: speedField
    text: viewModel.speedValue
    onEditingFinished: viewModel.setSpeed(text)
}

// ConfigViewModel.cpp
void ConfigViewModel::setSpeed(const QString& text) {
    bool ok;
    double value = text.toDouble(&ok);
    if (!ok || value < 0 || value > 3000) {
        emit inputError("转速范围: 0-3000 RPM");
        return;
    }
    paramService_->writeParameter(TopicId::MotorSpeed, value);
}
```

#### 阶段 2: Service 层处理

```cpp
void ParamManagerService::writeParameter(TopicId topic, Value value) {
    // 1. 权限检查
    if (!hasWritePermission(topic)) {
        emit permissionDenied(topic);
        return;
    }

    // 2. 构造命令
    Command cmd;
    cmd.id = nextCommandId();
    cmd.topic = topic;
    cmd.value = value;
    cmd.priority = CommandPriority::ConfigWrite;  // Priority 1
    cmd.timestamp = Clock::now();
    cmd.timeout_ms = 500;

    // 3. 入队
    databus_->enqueueCommand(cmd);

    // 4. 记录日志（脱敏）
    LOG_INFO("Write command: id={}, topic={}, priority=1", cmd.id, topic);
}
```

#### 阶段 3: CommandQueue 优先级

```cpp
class CommandQueue {
    // 使用三个独立队列，按优先级出队
    std::queue<Command> emergency_queue_;   // Priority 0
    std::queue<Command> config_queue_;      // Priority 1
    std::queue<Command> query_queue_;       // Priority 2

public:
    void push(Command cmd) {
        switch (cmd.priority) {
            case CommandPriority::EmergencyStop: emergency_queue_.push(cmd); break;
            case CommandPriority::ConfigWrite:   config_queue_.push(cmd);    break;
            case CommandPriority::QueryRead:      query_queue_.push(cmd);     break;
        }
    }

    std::optional<Command> pop() {
        if (!emergency_queue_.empty()) { auto c = emergency_queue_.front(); emergency_queue_.pop(); return c; }
        if (!config_queue_.empty())    { auto c = config_queue_.front(); config_queue_.pop();    return c; }
        if (!query_queue_.empty())      { auto c = query_queue_.front(); query_queue_.pop();      return c; }
        return std::nullopt;
    }
};
```

#### 阶段 4: Protocol 编码

```cpp
void ModbusProtocol::onCommand(Command cmd) {
    std::vector<uint8_t> frame;

    // 编码 Modbus 帧
    frame.push_back(device_addr_);           // 设备地址
    frame.push_back(function_code);          // 功能码 (0x06 = 写单个寄存器)
    frame.push_back((reg_addr >> 8) & 0xFF); // 寄存器地址高字节
    frame.push_back(reg_addr & 0xFF);        // 寄存器地址低字节
    frame.push_back((reg_value >> 8) & 0xFF);// 寄存器值高字节
    frame.push_back(reg_value & 0xFF);       // 寄存器值低字节

    // CRC16 计算
    uint16_t crc = crc16(frame.data(), frame.size());
    frame.push_back(crc & 0xFF);
    frame.push_back((crc >> 8) & 0xFF);

    // 发送
    transport_write_callback_(frame);
}
```

#### 阶段 5: Transport 发送

```cpp
void SerialTransport::write(std::span<const uint8_t> data) {
    serial_port_.write(reinterpret_cast<const char*>(data.data()), data.size());
    // 非阻塞写入，QtSerialPort 内部缓冲
}
```

---

## DataBus 发布/订阅模型

### 核心概念

```
┌─────────────────────────────────────────────────────────────────┐
│                        DataBus Architecture                      │
│                                                                  │
│  Producer (Protocol)                                             │
│       │                                                          │
│       │ publish(TopicId, DataPoint)                              │
│       ▼                                                          │
│  ┌─────────────────────────────────────────────────────────┐    │
│  │                    DataBus Core                           │    │
│  │                                                          │    │
│  │   TopicRegistry:  {TopicId → DataPointMeta}              │    │
│  │   SubscriptionMap: {TopicId → [Callback]}                │    │
│  │   RingBufferPool:  {TopicId → RingBuffer}                │    │
│  │   DataCache:       {TopicId → DataPoint}                 │    │
│  │                                                          │    │
│  └─────────────────────────────────────────────────────────┘    │
│       │         │         │                                      │
│       ▼         ▼         ▼                                      │
│  Subscriber Subscriber Subscriber                                │
│  (Monitor)  (Curve)    (Logger)                                  │
│                                                                  │
│  ★ 广播语义：每个 Subscriber 都收到同一份数据                     │
│  ★ 订阅者之间互不影响，不会"消费"数据                             │
└─────────────────────────────────────────────────────────────────┘
```

### 订阅者注册

```cpp
// 订阅者注册
SubscriptionId sub_id = databus->subscribe(
    TopicId::MotorSpeed,
    [](DataPoint dp) {
        // 处理数据
        updateDashboard(dp);
    }
);

// 订阅者注销
databus->unsubscribe(sub_id);

// 支持通配符订阅（可选）
SubscriptionId sub_id = databus->subscribe(
    TopicId::Wildcard,  // 订阅所有 Topic
    [](DataPoint dp) {
        logAllData(dp);
    }
);
```

### 订阅者回调线程

```cpp
// 回调在 DataBus Thread 中执行（默认）
// 订阅者应尽快返回，避免阻塞 DataBus
databus->subscribe(topic, [](DataPoint dp) {
    // ⚡ 快速处理，不要阻塞
    latest_value_ = dp.value;  // 原子操作，快速返回
});

// 如果处理耗时，应自行分发到 WorkerPool
databus->subscribe(topic, [this](DataPoint dp) {
    QThreadPool::globalInstance()->start([this, dp]() {
        heavyProcessing(dp);  // 耗时操作在 WorkerPool 执行
    });
});
```

---

## Topic 系统

### 整数 ID 设计

```cpp
// TopicId 使用整数枚举，编译期确定，O(1) 查找
enum class TopicId : uint32_t {
    // 系统状态 (0x0000 - 0x00FF)
    SystemStatus     = 0x0001,
    ConnectionState  = 0x0002,
    ErrorCode        = 0x0003,

    // 电机参数 (0x0100 - 0x01FF)
    MotorSpeed       = 0x0100,
    MotorCurrent     = 0x0101,
    MotorVoltage     = 0x0102,
    MotorTemperature = 0x0103,
    MotorTorque      = 0x0104,

    // 编码器数据 (0x0200 - 0x02FF)
    EncoderPosition  = 0x0200,
    EncoderVelocity  = 0x0201,

    // 驱动器参数 (0x0300 - 0x03FF)
    DriverStatus     = 0x0300,
    DriverFaultCode  = 0x0301,
    BusVoltage       = 0x0302,

    // 自定义扩展 (0x8000 - 0xFFFF)
    CustomBase       = 0x8000,
};

// 编译期 Topic 元数据
struct DataPointMeta {
    TopicId     id;
    const char* name;           // 人类可读名称（仅用于日志/UI）
    const char* unit;           // 单位
    double      min_value;      // 最小值
    double      max_value;      // 最大值
    uint32_t    update_rate_hz; // 预期更新频率
};
```

### 为什么不用字符串

| 方面 | 整数 ID | 字符串 |
|------|---------|--------|
| 比较 | O(1) 单条 CPU 指令 | O(n) 逐字符比较 |
| 哈希 | 无需哈希 | 需要哈希计算，有碰撞风险 |
| 内存 | 4 字节 | 动态分配，24+ 字节 |
| 数组索引 | 天然支持 | 需要哈希表映射 |
| 调试 | 需要查表 | 可读性好 |
| 扩展性 | 枚举/范围预定义 | 任意新增 |

**折中方案**：运行时使用整数 ID，调试/日志时通过 `TopicRegistry` 查找名称。

---

## RingBuffer 设计

### 设计目标

- **无锁**：单生产者（DataBus Thread），多消费者（订阅者）
- **零拷贝**：订阅者直接读取缓冲区，不拷贝数据
- **固定容量**：每个 Topic 独立 RingBuffer，容量可配置（默认 1MB）
- **溢出策略**：丢弃最旧数据，记录溢出计数

### 实现

```cpp
template<typename T, size_t Capacity>
class RingBuffer {
    static_assert((Capacity & (Capacity - 1)) == 0,
                  "Capacity must be power of 2 for modulo optimization");

    alignas(64) std::array<T, Capacity> buffer_;  // cache-line aligned
    alignas(64) std::atomic<uint64_t> write_seq_{0};

public:
    // ====== 生产者接口（单生产者） ======

    uint64_t push(const T& item) {
        auto seq = write_seq_.fetch_add(1, std::memory_order_acq_rel);
        buffer_[seq & (Capacity - 1)] = item;
        return seq;
    }

    // ====== 消费者接口（多消费者，无锁读） ======

    // 获取最新写入序号
    uint64_t latest_sequence() const {
        return write_seq_.load(std::memory_order_acquire);
    }

    // 读取最新数据
    const T* get_latest() const {
        auto seq = write_seq_.load(std::memory_order_acquire);
        if (seq == 0) return nullptr;
        return &buffer_[(seq - 1) & (Capacity - 1)];
    }

    // 批量读取历史数据
    std::vector<T> get_range(uint64_t from_seq, uint64_t to_seq) const {
        assert(to_seq >= from_seq);
        std::vector<T> result;
        result.reserve(to_seq - from_seq);
        for (auto seq = from_seq; seq < to_seq; ++seq) {
            result.push_back(buffer_[seq & (Capacity - 1)]);
        }
        return result;
    }

    // 容量信息
    constexpr size_t capacity() const { return Capacity; }
};

// 使用示例
RingBuffer<DataPoint, 1024 * 1024 / sizeof(DataPoint)> motor_speed_buffer;
// ~1MB 缓冲区，可存储约 16384 个 DataPoint
```

### 溢出处理

```cpp
class MonitoredRingBuffer : public RingBuffer<DataPoint, Capacity> {
    std::atomic<uint64_t> overflow_count_{0};
    std::atomic<uint64_t> total_pushed_{0};

public:
    uint64_t push(const DataPoint& item) {
        auto seq = RingBuffer::push(item);  // 总是覆盖最旧数据
        total_pushed_.fetch_add(1, std::memory_order_relaxed);

        // 检测溢出（如果最新序号 - 最早消费者序号 > Capacity）
        // 由外部监控线程定期检查
        return seq;
    }

    uint64_t getOverflowCount() const {
        return overflow_count_.load(std::memory_order_relaxed);
    }

    // 溢出策略（配置化）
    enum class OverrunPolicy {
        DropOldest,   // 丢弃最旧数据（默认）
        DropNewest,   // 丢弃最新数据
        Block         // 阻塞生产者（不推荐，影响实时性）
    };
};
```

---

## 性能目标

### 系统级指标

| 指标 | 目标值 | 说明 |
|------|--------|------|
| **并发监控变量数** | ≥ 100 | 同时监控 100+ 个变量 |
| **采样频率** | 1 kHz | 每个变量每秒 1000 次更新 |
| **端到端延迟（P99）** | < 1 ms | 从 MCU 发送到 UI 显示 |
| **延迟抖动** | < 100 μs | 最大延迟 - 最小延迟 |
| **数据吞吐量** | ≥ 200 KB/s | 100 变量 × 1kHz × 2 字节 |
| **CPU 占用（DataBus）** | < 5% | 单核 CPU 占用 |
| **内存占用（RingBuffer）** | < 128 MB | 100 变量 × 1MB 缓冲区 |

### 各阶段延迟预算

```
MCU → Transport:      < 50 μs  (串口传输时间 @ 115200 bps)
Transport → Protocol:  < 50 μs  (SPSC Queue + 回调)
Protocol → DataBus:    < 100 μs (帧解析 + CRC 校验)
DataBus → Subscriber:  < 100 μs (RingBuffer 写入 + 广播)
Subscriber → UI:       < 500 μs (Qt Signal/Slot + QML 渲染)
                           ─────
总计（P99）:            < 1 ms
```

### 性能测试方法

```cpp
// 延迟测试
void testEndToEndLatency() {
    auto start = Clock::now();

    // 模拟 MCU 发送数据
    simulateMCUData(topic_id, test_value);

    // 等待 UI 回调
    waitForUIUpdate([&]() {
        auto end = Clock::now();
        auto latency = end - start;
        REQUIRE(latency < std::chrono::milliseconds(1));
    });
}

// 吞吐量测试
void testThroughput() {
    constexpr int kNumVariables = 100;
    constexpr int kFrequency = 1000;  // 1 kHz
    constexpr int kDuration = 60;     // 60 秒

    auto start = Clock::now();
    for (int t = 0; t < kDuration * kFrequency; ++t) {
        for (int v = 0; v < kNumVariables; ++v) {
            databus->publish(topic_ids[v], generateDataPoint());
        }
    }
    auto end = Clock::now();

    auto throughput = (kNumVariables * kFrequency * kDuration) /
                      std::chrono::duration<double>(end - start).count();
    REQUIRE(throughput >= 100000);  // ≥ 100k msg/s
}
```

---

## 数据质量与可靠性

### 数据质量标记

```cpp
enum class Quality : uint8_t {
    Good       = 0x00,  // 正常数据
    Stale      = 0x01,  // 超时未更新（超过 max_age_ms）
    Uncertain  = 0x02,  // 不确定（CRC 重试后成功、通信抖动）
    Bad        = 0x03,  // 无效数据（CRC 失败、超范围）
};
```

### 超时检测

```cpp
// DataCache 中每个 DataPoint 携带 timestamp
// 订阅者可配置 max_age_ms
databus->subscribe(topic, [](DataPoint dp) {
    if (dp.quality == Quality::Stale) {
        // 数据过期，UI 显示灰色/闪烁
        showStaleIndicator(dp.topic_id);
    }
}, /*max_age_ms=*/100);
```

### 错误恢复

```
                    ┌─────────────┐
                    │  Transport  │
                    │  Error      │
                    └──────┬──────┘
                           │
              ┌────────────┼────────────┐
              ▼            ▼            ▼
        ┌──────────┐ ┌──────────┐ ┌──────────┐
        │ 重连     │ │ 切换备   │ │ 通知     │
        │ (3次)    │ │ 用通道   │ │ 上层     │
        └────┬─────┘ └────┬─────┘ └────┬─────┘
             │            │            │
             ▼            ▼            ▼
        ┌──────────────────────────────────┐
        │      DataBus: 标记所有 Topic     │
        │      Quality = Bad               │
        └──────────────────────────────────┘
```

---

## 后续实现注意事项

1. **Topic ID 分配管理**：在 `src/core/TopicIds.h` 中集中定义所有 Topic ID，按模块分段，CMake 扫描脚本自动检测冲突
2. **RingBuffer 容量调优**：初始 1MB/Topic，根据实际数据速率和消费速度动态调整；提供 `occupancy_ratio` 指标
3. **零拷贝的边界**：DataBus 广播时传递 `const DataPoint&` 引用，订阅者如需保存数据应自行拷贝，避免悬垂引用
4. **CommandQueue 超时处理**：命令发出后等待响应超时（默认 500ms），超时后自动重试 1 次，仍失败则通知上层
5. **DataBus 背压策略**：当 RingBuffer 满时，默认 `DropOldest`；紧急 Topic（如故障码）可配置为 `DropNewest` 保留最新数据
6. **订阅者回调异常处理**：每个订阅者回调包裹 `try/catch`，单个订阅者异常不影响其他订阅者
7. **性能指标导出**：DataBus 导出 `pub_count`、`sub_count`、`avg_latency_us`、`overflow_count` 等指标，通过 Topic 上报
8. **数据持久化**：可选的 DataLogger 订阅者，将指定 Topic 数据写入 SQLite/CSV 文件，用于离线分析
9. **时间戳精度**：使用 `std::chrono::steady_clock` 作为内部时间戳源，避免系统时间跳变影响；MCU 时间戳以 Transport 收到时刻为准
10. **协议层资源映射**：Modbus 寄存器地址到 Topic ID 的映射表应外部化（JSON/YAML 配置），不改代码即可适配不同电机