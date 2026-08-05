# 日志系统设计 (Logger Design)

## 目标

构建一个高性能、低延迟、分类清晰的日志系统，为 Motor Monitor 上位机提供开发和运维阶段的全方位诊断能力。日志系统需要在亚微秒级写入开销下，支持多类别、多输出通道、运行时级别切换、日志查询与导出功能。

---

## 设计原则

1. **零性能影响**：热路径日志写入必须无锁、无 malloc，单条日志写入开销 < 1μs。
2. **分类明确**：10 个日志类别覆盖开发调试到性能分析的所有场景，互不干扰。
3. **生产者-消费者分离**：前端（生产者）快速写入，后端（消费者）异步处理格式化与输出。
4. **可观测性**：日志自身不成为瓶颈，提供自监控（队列深度、丢弃计数、延迟）。
5. **边界清晰**：Logger 记录元数据与事件，Recorder 记录业务数据。两者职责分离。
6. **运行时可控**：支持运行时按类别、级别动态调整，无需重启程序。

---

## 类/模块关系

```
┌──────────────────────────────────────────────────────────────────────────┐
│                         LogManager (Singleton)                            │
│                                                                           │
│  - getLogger(category) → Logger&                                         │
│  - setLevel(category, level)                                             │
│  - getLevel(category) → LogLevel                                         │
│  - addSink(sink)                                                         │
│  - removeSink(sink)                                                      │
│  - flush()                                                               │
│  - shutdown()                                                            │
│  - query(filter) → LogQueryResult                                        │
│  - export(filter, format) → bool                                         │
└──────────────────┬───────────────────────────────────────────────────────┘
                   │ 1:1
                   ▼
┌──────────────────────────────────────────────────────────────────────────┐
│                          LogBuffer (Double-Buffered)                      │
│                                                                           │
│  ┌─────────────────────┐        ┌─────────────────────┐                  │
│  │   Front Buffer A    │ atomic │   Front Buffer B    │                  │
│  │   (active writes)   │ swap   │   (active writes)   │                  │
│  └─────────┬───────────┘        └─────────┬───────────┘                  │
│            │                              │                              │
│            │       ┌──────────────────────┘                              │
│            ▼       ▼                                                     │
│  ┌─────────────────────────────────────────────────────────────────┐     │
│  │                     Back Buffer (flush queue)                     │     │
│  │  - 4096 entries capacity                                         │     │
│  │  - atomic write index                                            │     │
│  │  - cache-line aligned (避免 false sharing)                       │     │
│  └─────────────────────────────┬───────────────────────────────────┘     │
│                                │ flush trigger: 4096 entries OR 10ms     │
│                                ▼                                          │
│  ┌─────────────────────────────────────────────────────────────────┐     │
│  │                     Sink Dispatcher (thread)                      │     │
│  │  - 读取 Back Buffer，分发到所有注册的 Sink                        │     │
│  └─────────────────────────────┬───────────────────────────────────┘     │
│                                │                                          │
└────────────────────────────────┼──────────────────────────────────────────┘
                                 │
         ┌───────────────────────┼───────────────────────┐
         ▼                       ▼                       ▼
┌─────────────────┐   ┌─────────────────┐   ┌─────────────────┐
│  ConsoleSink    │   │   FileSink      │   │  NetworkSink    │
│  (ANSI colors)  │   │  (per-category) │   │  (TCP/UDP)      │
└─────────────────┘   └─────────────────┘   └─────────────────┘
         │                       │                       │
         ▼                       ▼                       ▼
┌─────────────────┐   ┌─────────────────┐   ┌─────────────────┐
│    UISink       │   │  (Additional)   │   │  (Additional)   │
│  (ring buffer   │   │                 │   │                 │
│  + 100ms batch) │   │                 │   │                 │
└─────────────────┘   └─────────────────┘   └─────────────────┘
```

### 日志类别枚举

```cpp
enum class LogCategory : uint8_t {
    Debug        = 0,   // 开发调试信息
    Info         = 1,   // 一般运行信息
    Warning      = 2,   // 警告信息
    Error        = 3,   // 可恢复错误
    Critical     = 4,   // 严重错误（可能导致程序退出）
    Protocol     = 5,   // 通信协议日志（收发报文）
    Automation   = 6,   // 自动化测试步骤日志
    Device       = 7,   // 设备操作日志
    Curve        = 8,   // 波形/曲线数据日志
    Performance  = 9,   // 性能分析日志
};
```

### 日志级别

```cpp
enum class LogLevel : uint8_t {
    Off     = 0,   // 关闭
    Fatal   = 1,   // 仅 Critical
    Error   = 2,   // Error + Critical
    Warning = 3,   // Warning + Error + Critical
    Info    = 4,   // Info + Warning + Error + Critical
    Debug   = 5,   // 全部
    Trace   = 6,   // 全部 + 详细追踪
};
```

### 日志条目结构 (128B, cache-line 对齐)

```cpp
struct alignas(128) LogEntry {
    uint64_t    timestamp;          // 8B  - ns since epoch
    uint64_t    threadId;           // 8B  - thread identifier
    uint32_t    sequenceNumber;     // 4B  - 单调递增序号
    LogLevel    level;              // 1B  - 日志级别
    LogCategory category;           // 1B  - 日志类别
    uint16_t    messageLength;      // 2B  - 消息长度（不含截断标记）
    char        sourceFile[24];     // 24B - 源文件名（截断至24字符）
    uint32_t    sourceLine;         // 4B  - 源代码行号
    char        function[32];       // 32B - 函数名
    char        message[40];        // 40B - 消息前40字节（长消息截断）
    // padding: 剩余 4B 用于对齐
    // 总计: 128B
};
```

---

## 数据流

### 写入路径（热路径）

```
用户代码
  │
  ▼
LOG_DEBUG(category) << "message"
  │
  ▼
宏展开:
  if (level <= categoryLevel) {
      LogEntry entry;
      entry.timestamp = Clock::now();
      entry.category = category;
      entry.level = level;
      entry.messageLength = min(len, 40);
      memcpy(entry.message, msg, entry.messageLength);
      // ...
      LogManager::instance().write(entry);
  }
  │
  ▼
LogManager::write(entry):
  │
  ├─ 获取当前 Front Buffer (atomic read)
  │
  ├─ 原子递增 writeIndex
  │
  ├─ 如果 writeIndex < 4096:
  │     memcpy(buffer[writeIndex], &entry, 128)  // 128B 对齐写入
  │
  ├─ 如果 writeIndex == 4096:
  │     atomic swap Front ↔ Back Buffer
  │     通知 Sink Dispatcher 线程
  │     写入新 Front Buffer[0]
  │
  └─ 返回 (无锁，无 malloc，< 1μs)
```

### 消费路径（冷路径）

```
Sink Dispatcher 线程唤醒:
  │
  ├─ 触发条件: 4096 entries 满 OR 10ms 定时器到期
  │
  ├─ 获取 Back Buffer 中的有效条目数
  │
  ├─ 对每个条目，依次调用所有注册的 Sink
  │
  │   ConsoleSink::write(entry):
  │     ├─ 根据 level 选择 ANSI 颜色
  │     │   Debug=灰色, Info=绿色, Warning=黄色, Error=红色, Critical=红色+粗体
  │     │   Protocol=青色, Automation=洋红, Device=蓝色, Curve=暗绿, Perf=暗黄
  │     ├─ 格式化: [timestamp] [level] [category] [file:line] message
  │     └─ 输出到 stdout/stderr
  │
  │   FileSink::write(entry):
  │     ├─ 根据 category 路由到对应文件
  │     ├─ 检查是否需要滚动 (date rolling: 每天新文件; size rolling: >100MB)
  │     ├─ 文件名: {hostname}_{category}_{date}.log
  │     └─ 追加写入（带缓冲，64KB batch flush）
  │
  │   NetworkSink::write(entry):
  │     ├─ Critical → TCP (保证送达)
  │     ├─ 其他 → UDP (低延迟)
  │     └─ 格式: JSON 序列化
  │
  │   UISink::write(entry):
  │     ├─ 写入环形缓冲区 (ring buffer, 最多 10000 条)
  │     ├─ 100ms 定时器到期 → 批量刷新到 UI ListView
  │     └─ 更新统计: 各级别计数、丢弃计数
  │
  └─ 清空 Back Buffer，准备下一次 swap
```

### 运行时级别切换

```
用户通过 UI 或 API 调用:
  setLevel(LogCategory::Debug, LogLevel::Warning)
  │
  ▼
LogManager:
  ├─ 原子更新 per-category level 数组
  ├─ 后续 LOG_DEBUG 宏检查新 level，低于 Warning 的跳过
  └─ 无需刷新或重建任何数据结构
```

---

## API 接口规划

### 日志宏

```cpp
// 基础日志宏
#define LOG_DEBUG(cat)      LogStream(LogLevel::Debug,      cat, __FILE__, __LINE__, __FUNCTION__)
#define LOG_INFO(cat)       LogStream(LogLevel::Info,       cat, __FILE__, __LINE__, __FUNCTION__)
#define LOG_WARN(cat)       LogStream(LogLevel::Warning,    cat, __FILE__, __LINE__, __FUNCTION__)
#define LOG_ERROR(cat)      LogStream(LogLevel::Error,      cat, __FILE__, __LINE__, __FUNCTION__)
#define LOG_CRITICAL(cat)   LogStream(LogLevel::Fatal,      cat, __FILE__, __LINE__, __FUNCTION__)

// 专用宏
#define LOG_PROTOCOL_TX(payload, len)   LogManager::instance().logProtocol(true,  payload, len)
#define LOG_PROTOCOL_RX(payload, len)   LogManager::instance().logProtocol(false, payload, len)
#define LOG_PERF_SCOPE(name)            PerfScope _perf_scope_##__LINE__(name)
#define LOG_AUTOMATION_STEP(step)       LogStream(LogLevel::Info, LogCategory::Automation, ...) << step
#define LOG_DEVICE_OP(op, devId)        LogStream(LogLevel::Info, LogCategory::Device, ...) << op

// 使用示例
LOG_INFO(LogCategory::Device) << "Motor " << motorId << " started, speed=" << speed;
LOG_ERROR(LogCategory::Protocol) << "CRC check failed, expected=" << expected << " got=" << actual;
```

### LogStream 类

```cpp
class LogStream {
public:
    LogStream(LogLevel level, LogCategory category,
              const char* file, int line, const char* function);
    ~LogStream();  // 析构时自动提交日志条目

    template<typename T>
    LogStream& operator<<(const T& value);

    // 支持 std::endl, std::hex 等操纵符
    LogStream& operator<<(std::ostream& (*manip)(std::ostream&));

private:
    LogEntry entry_;
    char messageBuffer_[40];
    uint16_t written_;
    bool truncated_;
};
```

### LogManager 公共接口

```cpp
class LogManager {
public:
    static LogManager& instance();

    // 级别控制
    void setLevel(LogCategory category, LogLevel level);
    LogLevel getLevel(LogCategory category) const;
    void setAllLevels(LogLevel level);

    // Sink 管理
    void addSink(std::unique_ptr<LogSink> sink);
    void removeSink(LogSink* sink);
    std::vector<LogSink*> getSinks() const;

    // 内部写入（由宏调用）
    void write(const LogEntry& entry);
    void logProtocol(bool isTx, const uint8_t* payload, size_t len);

    // 刷新与控制
    void flush();
    void shutdown();

    // 查询
    LogQueryResult query(const LogQueryFilter& filter) const;
    bool exportLogs(const LogQueryFilter& filter, ExportFormat format,
                    const std::string& outputPath);

    // 自监控
    LogStats getStats() const;
};

// 查询过滤器
struct LogQueryFilter {
    std::optional<TimeRange> timeRange;
    std::optional<LogCategory> category;
    std::optional<LogLevel> minLevel;
    std::optional<std::string> keyword;
    std::optional<uint64_t> threadId;
    size_t maxResults = 1000;
    size_t offset = 0;
};

// 导出格式
enum class ExportFormat { Text, JSON, CSV, HTML };
```

### LogSink 基类

```cpp
class LogSink {
public:
    virtual ~LogSink() = default;
    virtual void write(const LogEntry& entry) = 0;
    virtual void flush() = 0;
    virtual std::string name() const = 0;
    virtual void shutdown() {}
};

// ConsoleSink
class ConsoleSink : public LogSink {
public:
    explicit ConsoleSink(bool useColors = true);
    void setColorEnabled(bool enabled);
    // ...
};

// FileSink
class FileSink : public LogSink {
public:
    struct Config {
        std::string directory;       // 日志目录
        std::string hostname;        // 主机名
        bool perCategory = true;     // 每个类别独立文件
        size_t maxFileSizeMB = 100;  // 单文件大小上限
        int maxBackupFiles = 30;     // 最大备份文件数
        bool dailyRolling = true;    // 每日滚动
        bool compressOld = true;     // 压缩旧文件
    };
    explicit FileSink(const Config& config);
    // ...
};

// NetworkSink
class NetworkSink : public LogSink {
public:
    struct Config {
        std::string host;
        uint16_t tcpPort = 0;  // 0 = 禁用 TCP
        uint16_t udpPort = 0;  // 0 = 禁用 UDP
        LogLevel tcpMinLevel = LogLevel::Fatal;  // TCP 只传 Critical
    };
    explicit NetworkSink(const Config& config);
    // ...
};

// UISink
class UISink : public LogSink {
public:
    explicit UISink(size_t ringBufferSize = 10000,
                    std::chrono::milliseconds batchInterval = 100ms);
    // 获取环形缓冲区内容（供 UI 线程读取）
    std::vector<LogEntry> getRecentEntries(size_t maxCount = 500) const;
    // 信号：有新日志批次到达
    Signal<const std::vector<LogEntry>&> onNewBatch;
    // ...
};
```

---

## 文件命名规范

```
{hostname}_{category}_{date}.log

示例:
  motor-pc_Debug_2026-08-05.log
  motor-pc_Protocol_2026-08-05.log
  motor-pc_Error_2026-08-05.log
  motor-pc_Device_2026-08-05.log

滚动后:
  motor-pc_Debug_2026-08-04.log       (前一天)
  motor-pc_Debug_2026-08-04.log.1.gz  (更早的，压缩)
```

---

## 长消息截断

```
原始消息 (80 字节):
  "Failed to read register 0x3F2A from device 'Motor_Controller_01' on port COM3"

LogEntry.message (40 字节):
  "Failed to read register 0x3F2A from d…█"  (末尾添加截断标记 '█' (U+2588))

格式化输出时:
  [2026-08-05 08:44:00.123456] [ERROR] [Device] [device_driver.cpp:342] Failed to read register 0x3F2A from d…█ <truncated, 80 bytes total>
```

---

## Logger 与 Recorder 职责边界

| 维度 | Logger | Recorder |
|------|--------|----------|
| **记录内容** | 元数据、事件、状态变化、错误 | 业务数据（电机参数、波形） |
| **数据量** | 低（KB/s） | 高（MB/s） |
| **存储格式** | 文本日志文件 | 二进制数据文件 (.rec) |
| **查询方式** | 文本搜索 | 时间索引 + 参数索引 |
| **保留策略** | 滚动删除 | 按项目/会话管理 |
| **典型场景** | "连接失败" | "转速 3000rpm @ t=1.234s" |

---

## 后续实现注意事项

1. **False Sharing 防止**：LogEntry 使用 `alignas(128)` 确保每个条目独占一个 cache line。环形缓冲区索引也使用独立 cache line 的原子变量。

2. **原子操作选择**：优先使用 `std::atomic` 的 `memory_order_acquire`/`memory_order_release`，避免 `memory_order_seq_cst` 的性能开销。

3. **线程模型**：Sink Dispatcher 使用独立线程，优先级低于主线程和 IO 线程。在 `shutdown()` 时确保所有缓冲区已刷新。

4. **FileSink 缓冲区**：每个文件使用独立的 `FILE*` 和 64KB 用户态缓冲区，避免频繁系统调用。`fflush` 在缓冲区满或 500ms 超时时触发。

5. **ANSI 颜色兼容性**：ConsoleSink 在 Windows 上需启用 `ENABLE_VIRTUAL_TERMINAL_PROCESSING`（Windows 10 build 14393+）。旧版本使用 `SetConsoleTextAttribute`。

6. **日志级别继承**：如果未对特定 category 设置级别，继承全局级别。全局级别默认为 Info。

7. **自监控指标**：记录每分钟写入速率、缓冲区满次数、丢弃次数、Sink 延迟。通过 `getStats()` 暴露，并在 UI 中展示。

8. **网络传输可靠性**：TCP Sink 需处理断线重连（指数退避），UDP Sink 接受丢包。网络连接状态在 UI 中指示。

9. **性能测试基线**：单条日志写入延迟 < 1μs (P99)，4096 条目批量刷新延迟 < 100μs，10 万条/秒持续写入不影响主线程帧率（60fps）。

10. **日志查询实现**：FileSink 支持基于时间范围二分查找（日志按时间有序），UISink 支持内存内线性扫描。查询结果分页返回。

11. **跨平台文件路径**：使用 `std::filesystem::path` 处理路径，日志目录默认为 `{AppData}/MotorMonitor/logs/`。

12. **初始化顺序**：LogManager 必须是第一个初始化的子系统（在 main() 早期），确保后续所有模块都能正常记录日志。使用 `std::atexit` 或 `QCoreApplication::aboutToQuit` 确保正常刷新。