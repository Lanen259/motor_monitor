# ModuleDesign — 模块设计目录

> **文档版本**: v1.0  
> **父文档**: [SystemArchitecture.md](SystemArchitecture.md)  

---

## 目标

定义 Motor Studio 全部 14 个核心模块的职责、接口、依赖关系、生命周期和实例化策略。为每个模块提供清晰的边界定义，确保模块间协作正确、可测试、可替换。

---

## 设计原则

| 原则 | 说明 |
|------|------|
| **高内聚低耦合** | 模块内部高度相关，模块间通过抽象接口交互 |
| **接口隔离** | 每个模块只暴露调用者真正需要的接口，不暴露实现细节 |
| **显式依赖** | 依赖通过构造函数注入，不使用全局变量或服务定位器 |
| **可替换性** | 任何模块实现可被 Mock 替换，支持独立单元测试 |
| **生命周期明确** | 每个模块有明确的 `init()` / `shutdown()` 调用点 |

---

## 模块总览

```
┌──────────────────────────────────────────────────────────────────┐
│  #   Module                    Layer          Instances  Thread  │
├──────────────────────────────────────────────────────────────────┤
│  1   TransportManager          Infrastructure  Single    Comm    │
│  2   SerialTransport           Infrastructure  Multi     Comm    │
│  3   CANTransport              Infrastructure  Multi     Comm    │
│  4   TCPTransport              Infrastructure  Multi     Comm    │
│  5   ConfigManager             Infrastructure  Single    Main    │
│  6   Logger                    Infrastructure  Single    Logger  │
│  7   PluginLoader              Infrastructure  Single    Main    │
│  8   CrashHandler              Infrastructure  Single    Main    │
│  9   DataBus                   Core             Single    DataBus │
│  10  ProtocolManager           Core             Single    DataBus │
│  11  ModbusProtocol            Protocol         Multi     DataBus │
│  12  DataMonitorService        Service          Single    Worker  │
│  13  CurveEngineService        Service          Single    Worker  │
│  14  AutomationEngineService   Service          Single    Worker  │
└──────────────────────────────────────────────────────────────────┘
```

---

## 模块依赖图

```
                    ┌──────────────────────┐
                    │    CrashHandler      │
                    └──────────┬───────────┘
                               │ (signal handler)
    ┌──────────────────────────┼──────────────────────────┐
    │                          │                          │
    ▼                          ▼                          ▼
┌──────────┐  ┌────────────────────────────┐  ┌──────────────────┐
│  Logger  │◄─┤       ConfigManager        │  │  PluginLoader    │
└──────────┘  └────────────┬───────────────┘  └────────┬─────────┘
                            │                           │
                            ▼                           │
              ┌─────────────────────────┐               │
              │    TransportManager     │◄──────────────┘
              └────────────┬────────────┘
                           │
              ┌────────────┼────────────┐
              │            │            │
              ▼            ▼            ▼
        ┌──────────┐ ┌──────────┐ ┌──────────┐
        │  Serial  │ │   CAN    │ │   TCP    │
        │Transport │ │Transport │ │Transport │
        └────┬─────┘ └────┬─────┘ └────┬─────┘
             │            │            │
             └────────────┼────────────┘
                          │
                          ▼
              ┌─────────────────────────┐
              │    ProtocolManager      │
              └────────────┬────────────┘
                           │
              ┌────────────┼────────────┐
              │            │            │
              ▼            ▼            ▼
        ┌──────────┐ ┌──────────┐ ┌──────────┐
        │  Modbus  │ │ CANopen  │ │ EtherCAT │
        │ Protocol │ │ Protocol │ │ Protocol │
        └────┬─────┘ └────┬─────┘ └────┬─────┘
             │            │            │
             └────────────┼────────────┘
                          │
                          ▼
              ┌─────────────────────────┐
              │        DataBus          │
              └────────────┬────────────┘
                           │
         ┌─────────────────┼─────────────────┐
         │                 │                 │
         ▼                 ▼                 ▼
┌─────────────────┐ ┌──────────────┐ ┌──────────────┐
│ DataMonitor     │ │ CurveEngine  │ │ Automation   │
│ Service         │ │ Service      │ │ Engine       │
└────────┬────────┘ └──────┬───────┘ └──────┬───────┘
         │                 │                 │
         └─────────────────┼─────────────────┘
                           │
                           ▼
                    ┌──────────────┐
                    │   UI Layer   │
                    │  (ViewModels)│
                    └──────────────┘
```

---

## 模块详细定义

---

### 1. TransportManager

| 属性 | 值 |
|------|-----|
| **层级** | Infrastructure |
| **实例** | Singleton |
| **线程** | Main Thread |
| **依赖** | ConfigManager, PluginLoader |

**职责**：
- 管理所有传输实例的生命周期（创建/销毁/重连）
- 根据配置创建对应的 Transport 实例（Serial/CAN/TCP）
- 提供统一的 `openConnection(ConnectionConfig)` / `closeConnection(ConnectionId)` 接口
- 连接状态监控与自动重连

**接口**：
```cpp
class ITransportManager {
public:
    virtual ~ITransportManager() = default;
    virtual ConnectionId openConnection(const ConnectionConfig& cfg) = 0;
    virtual void closeConnection(ConnectionId id) = 0;
    virtual std::vector<ConnectionInfo> listConnections() const = 0;
    virtual void registerTransportFactory(const std::string& type, TransportFactory factory) = 0;
};
```

**生命周期**：
```
init() → registerDefaultFactories() → (runtime: open/close connections) → shutdown()
```

---

### 2. SerialTransport

| 属性 | 值 |
|------|-----|
| **层级** | Infrastructure |
| **实例** | Multi-instance（每个串口连接一个实例） |
| **线程** | Communication Thread（独立线程） |
| **依赖** | ITransport 接口, QtSerialPort |

**职责**：
- 封装 QtSerialPort，提供异步读写
- 波特率/数据位/停止位/校验位配置
- 字节流收发，不做协议解析

**接口**：实现 `ITransport`
```cpp
class ITransport {
public:
    virtual ~ITransport() = default;
    virtual bool open(const TransportConfig& cfg) = 0;
    virtual void close() = 0;
    virtual bool isOpen() const = 0;
    virtual size_t write(std::span<const uint8_t> data) = 0;
    // 通过回调返回数据，不阻塞
    virtual void setReadCallback(ReadCallback cb) = 0;
    virtual void setErrorCallback(ErrorCallback cb) = 0;
};
```

---

### 3. CANTransport

| 属性 | 值 |
|------|-----|
| **层级** | Infrastructure |
| **实例** | Multi-instance |
| **线程** | Communication Thread |
| **依赖** | ITransport, QCanBus |

**职责**：CAN 总线通信，帧收发，波特率配置，过滤规则设置

---

### 4. TCPTransport

| 属性 | 值 |
|------|-----|
| **层级** | Infrastructure |
| **实例** | Multi-instance |
| **线程** | Communication Thread |
| **依赖** | ITransport, ASIO |

**职责**：TCP 客户端/服务器模式通信，支持断线重连，心跳保活

---

### 5. ConfigManager

| 属性 | 值 |
|------|-----|
| **层级** | Infrastructure |
| **实例** | Singleton |
| **线程** | Main Thread（线程安全读，写操作需加锁） |
| **依赖** | 无（仅依赖 std::filesystem） |

**职责**：
- JSON/YAML 配置文件读写
- 默认值合并（用户配置 ← 默认配置）
- 配置变更通知（`ConfigChanged` signal）
- 热加载支持

**接口**：
```cpp
class IConfigManager {
public:
    virtual ~IConfigManager() = default;
    virtual void load(const std::filesystem::path& path) = 0;
    virtual void save() = 0;
    virtual void reload() = 0;  // 热加载

    template<typename T>
    T get(const std::string& key, const T& default_value = {}) const;

    template<typename T>
    void set(const std::string& key, const T& value);

    virtual void subscribe(const std::string& key, ConfigCallback cb) = 0;
};
```

---

### 6. Logger

| 属性 | 值 |
|------|-----|
| **层级** | Infrastructure |
| **实例** | Singleton |
| **线程** | Logger Thread（独立线程，异步写盘） |
| **依赖** | spdlog |

**职责**：
- 异步日志写入（文件 + 控制台 + 可选网络 sink）
- 按日轮转，保留最近 N 天
- 日志级别动态调整
- 敏感信息脱敏

**接口**：
```cpp
class Logger {
public:
    static Logger& instance();
    void init(const LoggerConfig& cfg);
    void shutdown();

    // 宏封装，自动捕获文件名/行号
    void log(Level lvl, std::string_view msg, const SourceLoc& loc = {});

    // 便捷宏
    #define LOG_TRACE(msg)   Logger::instance().log(Level::Trace, msg)
    #define LOG_INFO(msg)    Logger::instance().log(Level::Info, msg)
    #define LOG_ERROR(msg)   Logger::instance().log(Level::Error, msg)
};
```

---

### 7. PluginLoader

| 属性 | 值 |
|------|-----|
| **层级** | Infrastructure |
| **实例** | Singleton |
| **线程** | Main Thread |
| **依赖** | ConfigManager |

**职责**：
- 扫描插件目录，加载 `.dll`/`.so` 插件
- 版本校验（`api_version` 比对）
- 调用插件 `register` 函数注册工厂
- 符号解析与安全卸载

**接口**：
```cpp
class PluginLoader {
public:
    void scanDirectory(const std::filesystem::path& dir);
    std::vector<PluginInfo> listPlugins() const;
    void* loadSymbol(const std::string& plugin_name, const std::string& symbol);
    void unloadAll();
};
```

---

### 8. CrashHandler

| 属性 | 值 |
|------|-----|
| **层级** | Infrastructure |
| **实例** | Singleton |
| **线程** | Main Thread（信号处理在独立栈） |
| **依赖** | Logger |

**职责**：
- 注册 SIGSEGV / SIGABRT / SIGFPE 信号处理
- 生成 minidump (Windows) / core dump (Linux)
- 崩溃日志写入，记录调用栈
- 崩溃恢复提示

---

### 9. DataBus

| 属性 | 值 |
|------|-----|
| **层级** | Core |
| **实例** | Singleton |
| **线程** | DataBus Thread（独立线程） |
| **依赖** | 无 |

**职责**：
- 发布/订阅引擎：Topic 注册、订阅管理、消息分发
- RingBuffer 管理：零拷贝环形缓冲区
- DataCache 管理：最新值缓存 + 过期检测
- CommandQueue 管理：优先级命令队列
- Watchdog 心跳

**接口**：
```cpp
class IDataBus {
public:
    virtual ~IDataBus() = default;

    // 发布/订阅
    virtual TopicId registerTopic(const DataPointMeta& meta) = 0;
    virtual SubscriptionId subscribe(TopicId topic, SubscriberCallback cb) = 0;
    virtual void unsubscribe(SubscriptionId id) = 0;
    virtual void publish(TopicId topic, DataPoint data) = 0;

    // 数据查询
    virtual std::optional<DataPoint> getLatest(TopicId topic) const = 0;
    virtual std::vector<DataPoint> getHistory(TopicId topic, Timestamp from, Timestamp to) const = 0;

    // 命令
    virtual void enqueueCommand(Command cmd) = 0;
    virtual std::optional<Command> dequeueCommand() = 0;

    // 生命周期
    virtual void init(const DataBusConfig& cfg) = 0;
    virtual void shutdown() = 0;

    // 健康检查
    virtual bool isHealthy() const = 0;
    virtual uint64_t getHeartbeatCount() const = 0;
};
```

---

### 10. ProtocolManager

| 属性 | 值 |
|------|-----|
| **层级** | Core |
| **实例** | Singleton |
| **线程** | DataBus Thread |
| **依赖** | DataBus, TransportManager |

**职责**：
- 管理协议实例生命周期
- 数据路由：Transport → Protocol → DataBus（上行）
- 命令路由：CommandQueue → Protocol → Transport（下行）
- 协议自动检测（可选）

**接口**：
```cpp
class ProtocolManager {
public:
    void init(std::shared_ptr<IDataBus> bus, std::shared_ptr<ITransportManager> transport);
    void registerProtocol(const std::string& name, ProtocolFactory factory);
    void attachProtocol(ConnectionId conn_id, const std::string& protocol_name);
    void detachProtocol(ConnectionId conn_id);
    void shutdown();
};
```

---

### 11. ModbusProtocol

| 属性 | 值 |
|------|-----|
| **层级** | Protocol |
| **实例** | Multi-instance（每个连接一个实例） |
| **线程** | DataBus Thread |
| **依赖** | IProtocol 接口 |

**职责**：
- Modbus RTU/ASCII/TCP 帧解析与编码
- 功能码 03/06/10/16 支持
- 寄存器映射到 DataBus Topic
- 超时/重试/CRC 校验

**接口**：实现 `IProtocol`
```cpp
class IProtocol {
public:
    virtual ~IProtocol() = default;
    virtual void init(const ProtocolConfig& cfg, std::shared_ptr<IDataBus> bus) = 0;
    virtual void onBytesReceived(std::span<const uint8_t> data) = 0;
    virtual void onCommand(Command cmd) = 0;
    virtual void setTransportWriteCallback(WriteCallback cb) = 0;
    virtual void shutdown() = 0;
};
```

---

### 12. DataMonitorService

| 属性 | 值 |
|------|-----|
| **层级** | Service |
| **实例** | Singleton |
| **线程** | WorkerPool |
| **依赖** | IDataBus |

**职责**：
- 订阅 DataBus 关注变量
- 计算衍生指标（平均值、标准差、变化率）
- 阈值告警触发
- 数据快照存储

**接口**：
```cpp
class DataMonitorService {
public:
    void init(std::shared_ptr<IDataBus> bus);
    void addWatchVariable(TopicId topic, const WatchConfig& cfg);
    void removeWatchVariable(TopicId topic);
    void setThreshold(TopicId topic, double min, double max);
    Signal<std::vector<AlarmEvent>> onAlarm;
    void shutdown();
};
```

---

### 13. CurveEngineService

| 属性 | 值 |
|------|-----|
| **层级** | Service |
| **实例** | Singleton |
| **线程** | WorkerPool |
| **依赖** | IDataBus |

**职责**：
- 波形数据缓存管理（可配置缓冲区大小）
- 渲染数据准备（降采样、插值）
- 缩放/平移数据窗口计算
- 多通道同步

**接口**：
```cpp
class CurveEngineService {
public:
    void init(std::shared_ptr<IDataBus> bus);
    void addChannel(TopicId topic, const ChannelConfig& cfg);
    void removeChannel(TopicId topic);
    CurveData getRenderData(const RenderRequest& req);
    Signal<CurveData> onNewData;
    void shutdown();
};
```

---

### 14. AutomationEngineService

| 属性 | 值 |
|------|-----|
| **层级** | Service |
| **实例** | Singleton |
| **线程** | WorkerPool |
| **依赖** | IDataBus, ParamManagerService |

**职责**：
- 脚本解析与执行（Python/Lua/自定义 DSL）
- 自动化测试序列编排
- 结果收集与报告生成
- 紧急停止处理

**接口**：
```cpp
class AutomationEngineService {
public:
    void init(std::shared_ptr<IDataBus> bus, std::shared_ptr<IParamManager> param);
    void loadScript(const std::filesystem::path& path);
    void run();
    void stop();
    void pause();
    void resume();
    Signal<ScriptState> onStateChanged;
    Signal<ScriptResult> onCompleted;
    void shutdown();
};
```

---

## 模块版本兼容机制

### 接口版本控制

```cpp
// 每个接口定义版本号
class IDataBus {
public:
    static constexpr int API_VERSION = 1;  // 主版本号
    static constexpr int API_REVISION = 0; // 修订号

    // 运行时版本校验
    virtual int getApiVersion() const { return API_VERSION; }
    virtual int getApiRevision() const { return API_REVISION; }
};
```

### 插件版本校验

```json
// plugin_manifest.json
{
    "name": "modbus_protocol",
    "version": "1.2.0",
    "api_version": 1,
    "api_revision": 0,
    "min_host_version": "1.0.0",
    "max_host_version": "2.0.0"
}
```

### 兼容性规则

| 场景 | 规则 |
|------|------|
| `api_version` 不匹配 | 拒绝加载，日志记录 ERROR |
| `api_version` 匹配 + `api_revision` 更高 | 警告加载，可能功能缺失 |
| 插件版本 < `min_host_version` | 拒绝加载 |
| 插件版本 > `max_host_version` | 警告加载 |

---

## 扩展点

### 传输层扩展

```cpp
// 注册新的传输类型
class TransportFactory {
public:
    virtual std::unique_ptr<ITransport> create(const TransportConfig& cfg) = 0;
    virtual std::string typeName() const = 0;
};

// 使用方式
transportManager->registerTransportFactory("bluetooth", std::make_unique<BluetoothTransportFactory>());
```

### 协议层扩展

```cpp
// 注册新的协议类型
class ProtocolFactory {
public:
    virtual std::unique_ptr<IProtocol> create(const ProtocolConfig& cfg) = 0;
    virtual std::string typeName() const = 0;
};

// 使用方式
protocolManager->registerProtocol("ethercat", std::make_unique<EtherCATProtocolFactory>());
```

### Service 层扩展

```cpp
// Service 通过 DataBus 订阅即可接入，无需注册
class MyCustomService {
public:
    void init(std::shared_ptr<IDataBus> bus) {
        bus->subscribe(topic_id, [this](DataPoint dp) { onData(dp); });
    }
};
```

### UI 扩展

```cpp
// UI 通过 QML Plugin 注册新组件
// MyCustomPanel.qml 放在 qml/ 目录下即可被 QML Engine 发现
```

---

## 后续实现注意事项

1. **模块初始化顺序**：必须严格按 `ConfigManager → Logger → CrashHandler → PluginLoader → TransportManager → ProtocolManager → DataBus → Service Layer → UI Layer` 顺序初始化
2. **模块关闭顺序**：初始化顺序的逆序：`UI → Service → DataBus → Protocol → Transport → PluginLoader → CrashHandler → Logger`
3. **单例安全**：所有 Singleton 使用 `std::call_once` 或 C++11 Magic Statics，确保线程安全
4. **接口 Mock**：每个 `I*` 接口提供对应的 `Mock*` 实现（如 `MockDataBus`），放入 `tests/mocks/`
5. **工厂注册时机**：静态注册（`static RegisterFactory`）在 `main()` 之前完成，确保 `PluginLoader` 加载前内置工厂已就绪
6. **Watchdog 监控范围**：Watchdog 监控 DataBus Thread 心跳；若超时 3s，触发 `onWatchdogTimeout` 回调，由 Application 层决策（重启 DataBus 或全局退出）
7. **多实例模块的线程安全**：SerialTransport/CANTransport 等 Multi-instance 模块各自运行在独立线程，内部无需加锁，但共享资源（如 DataBus 引用）必须线程安全
8. **DataBus 作为唯一数据通道**：禁止 Service 层直接调用 Protocol 层获取数据，必须通过 DataBus 中转
9. **CommandQueue 优先级**：紧急停止(0) > 配置写入(1) > 查询读取(2)，确保紧急命令不排队
10. **模块热替换**：PluginLoader 支持运行时卸载/重载协议插件，但传输插件只能在无连接时替换