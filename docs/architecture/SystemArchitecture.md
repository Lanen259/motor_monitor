# SystemArchitecture — 系统架构设计

> **文档版本**: v1.0  
> **父文档**: [README.md](../README.md)  

---

## 目标

定义 Motor Studio 的系统级架构，包括分层模型、依赖规则、技术选型依据、组件交互方式和跨平台策略。本文档是后续模块设计、线程模型、数据流设计的**顶层约束文件**。

---

## 设计原则

### 架构原则

| 原则 | 说明 |
|------|------|
| **分层隔离** | 严格四层架构，禁止跨层调用 |
| **单向依赖** | 依赖方向：UI → Service → Core → Infrastructure（上层依赖下层，下层不感知上层） |
| **接口契约** | 层间通信仅通过抽象接口（抽象基类/虚接口），不依赖具体实现 |
| **开闭原则** | 对扩展开放（新增协议/传输/UI 组件），对修改关闭（核心层稳定） |
| **单一职责** | 每个模块只做一件事，通过组合实现复杂功能 |
| **依赖注入** | 模块通过构造函数/工厂注入依赖，不使用全局变量传递 |
| **错误隔离** | 单模块故障不影响其他模块；崩溃时 Core Dump + 自动重启 |

### 性能原则

| 原则 | 目标 |
|------|------|
| **零拷贝优先** | 数据在层间传递时使用 `std::span` / 共享内存，避免拷贝 |
| **无锁优先** | 热路径使用 lock-free 数据结构，次热路径使用 `std::shared_mutex` |
| **批量处理** | 数据批量打包传输，减少系统调用次数 |
| **延迟可预测** | 关键路径延迟 < 1ms（P99），抖动 < 100μs |

---

## 四层架构

```
┌──────────────────────────────────────────────────────────────────┐
│                        Layer 4: UI Layer                          │
│  职责：用户交互、数据可视化、参数配置、自动化脚本编辑               │
│  技术：Qt QML 6.6+ / C++ ViewModel / Qt Charts / Qt Quick 3D     │
│  依赖：Layer 3 (Service)                                          │
├──────────────────────────────────────────────────────────────────┤
│                      Layer 3: Service Layer                       │
│  职责：业务逻辑编排、会话管理、数据聚合、曲线渲染、自动化执行       │
│  技术：C++20 / Qt Signals & Slots / QThreadPool                   │
│  依赖：Layer 2 (Core)                                             │
├──────────────────────────────────────────────────────────────────┤
│                   Layer 2: Core / Protocol Layer                  │
│  职责：数据总线、协议解析、命令路由、数据缓存、Topic 管理           │
│  技术：C++20 / Lock-free Queue / RingBuffer / Protobuf            │
│  依赖：Layer 1 (Infrastructure)                                   │
├──────────────────────────────────────────────────────────────────┤
│                 Layer 1: Infrastructure Layer                     │
│  职责：硬件通信、配置管理、日志系统、插件加载、崩溃处理             │
│  技术：C++20 / spdlog / Qt Core / ASIO / OS API                   │
│  依赖：操作系统 API / 第三方库                                     │
└──────────────────────────────────────────────────────────────────┘
```

### 每层详细职责

#### Layer 1: Infrastructure（基础设施层）

```
┌─────────────────────────────────────────────────────────────┐
│                    Infrastructure Layer                       │
│                                                              │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌─────────────┐ │
│  │ Transport │  │  Config  │  │  Logger  │  │   Plugin    │ │
│  │ Abstract  │  │  Manager │  │ (spdlog) │  │   Loader    │ │
│  └──────────┘  └──────────┘  └──────────┘  └─────────────┘ │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌─────────────┐ │
│  │  Serial  │  │   CAN    │  │   TCP    │  │  Crash      │ │
│  │ Transport│  │ Transport│  │ Transport│  │  Handler    │ │
│  └──────────┘  └──────────┘  └──────────┘  └─────────────┘ │
│  ┌──────────┐  ┌──────────┐                                 │
│  │   USB    │  │  Custom  │  ← 可扩展                        │
│  │ Transport│  │ Transport│                                 │
│  └──────────┘  └──────────┘                                 │
└─────────────────────────────────────────────────────────────┘
```

- **Transport Layer**: 抽象物理通信接口，提供统一的 `open/close/read/write` 接口
- **ConfigManager**: JSON/YAML 配置读写，热加载支持
- **Logger**: 异步日志，支持多 sink（文件/控制台/网络）
- **PluginLoader**: 动态加载 .so/.dll 插件，版本校验
- **CrashHandler**: 信号捕获，Core Dump 生成，崩溃报告

#### Layer 2: Core / Protocol（核心/协议层）

```
┌─────────────────────────────────────────────────────────────┐
│                    Core / Protocol Layer                      │
│                                                              │
│  ┌──────────────────────────────────────────────────────┐   │
│  │                      DataBus                           │   │
│  │  ┌──────────┐  ┌──────────┐  ┌────────────────────┐  │   │
│  │  │  Pub/Sub │  │RingBuffer│  │   Topic Registry   │  │   │
│  │  │  Engine  │  │  (lock-  │  │   (int→DataPoint)  │  │   │
│  │  │          │  │   free)  │  │                    │  │   │
│  │  └──────────┘  └──────────┘  └────────────────────┘  │   │
│  │  ┌──────────┐  ┌──────────┐                           │   │
│  │  │  Data    │  │ Command  │                           │   │
│  │  │  Cache   │  │  Queue   │                           │   │
│  │  └──────────┘  └──────────┘                           │   │
│  └──────────────────────────────────────────────────────┘   │
│                                                              │
│  ┌──────────────────────────────────────────────────────┐   │
│  │                   Protocol Layer                       │   │
│  │  ┌──────────┐  ┌──────────┐  ┌──────────┐            │   │
│  │  │  Modbus  │  │ CANopen  │  │ EtherCAT │            │   │
│  │  │ Protocol │  │ Protocol │  │ Protocol │            │   │
│  │  └──────────┘  └──────────┘  └──────────┘            │   │
│  │  ┌──────────┐  ┌──────────┐                           │   │
│  │  │  Custom  │  │  Future  │  ← 可扩展                  │   │
│  │  │ Protocol │  │ Protocol │                           │   │
│  │  └──────────┘  └──────────┘                           │   │
│  └──────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────┘
```

- **DataBus**: 系统数据中枢，实现发布/订阅模式，Topic 管理，数据缓存
- **Protocol Layer**: 协议解析与编码，将原始字节流转换为结构化 DataPoint

#### Layer 3: Service（服务层）

```
┌─────────────────────────────────────────────────────────────┐
│                       Service Layer                           │
│                                                              │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌─────────────┐ │
│  │  Data    │  │  Curve   │  │  Param   │  │ Automation  │ │
│  │ Monitor  │  │  Engine  │  │  Manager │  │   Engine    │ │
│  │ Service  │  │  Service │  │  Service │  │   Service   │ │
│  └──────────┘  └──────────┘  └──────────┘  └─────────────┘ │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐                  │
│  │  Logger  │  │ Session  │  │  Alarm   │                  │
│  │  Service │  │  Manager │  │  Service │                  │
│  └──────────┘  └──────────┘  └──────────┘                  │
└─────────────────────────────────────────────────────────────┘
```

- **DataMonitorService**: 订阅 DataBus 数据变化，计算衍生指标，触发告警
- **CurveEngineService**: 波形数据缓存、渲染调度、缩放/平移处理
- **ParamManagerService**: 参数读写、批量配置、参数版本管理
- **AutomationEngineService**: 脚本解析、执行调度、结果收集
- **LoggerService**: 业务日志记录，关键操作审计
- **SessionManager**: 连接会话生命周期管理
- **AlarmService**: 告警规则引擎，阈值判断，告警分级

#### Layer 4: UI（用户界面层）

```
┌─────────────────────────────────────────────────────────────┐
│                         UI Layer                              │
│                                                              │
│  ┌──────────────────────────────────────────────────────┐   │
│  │                    QML Views                          │   │
│  │  ┌──────────┐  ┌──────────┐  ┌──────────┐            │   │
│  │  │Dashboard │  │  Scope   │  │  Config  │            │   │
│  │  │  View    │  │  View    │  │  View    │            │   │
│  │  └──────────┘  └──────────┘  └──────────┘            │   │
│  │  ┌──────────┐  ┌──────────┐  ┌──────────┐            │   │
│  │  │Automation│  │ LogView  │  │ Settings │            │   │
│  │  │  View    │  │          │  │  View    │            │   │
│  │  └──────────┘  └──────────┘  └──────────┘            │   │
│  └──────────────────────────────────────────────────────┘   │
│                                                              │
│  ┌──────────────────────────────────────────────────────┐   │
│  │                  C++ ViewModels                       │   │
│  │  ┌──────────┐  ┌──────────┐  ┌──────────┐            │   │
│  │  │Dashboard │  │  Scope   │  │  Config  │            │   │
│  │  │ViewModel │  │ViewModel │  │ViewModel │            │   │
│  │  └──────────┘  └──────────┘  └──────────┘            │   │
│  └──────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────┘
```

- **MVVM 模式**: QML View 纯展示，C++ ViewModel 处理逻辑和状态
- **Qt Signals/Slots**: ViewModel 通过 signal 通知 View 更新
- **Qt Quick Controls**: 统一 UI 组件库，跨平台一致外观

---

## 依赖规则

### 依赖方向

```
UI  ──────────►  Service  ──────────►  Core  ──────────►  Infrastructure
(上层)           (中层)               (下层)              (底层)

依赖方向：上层依赖下层，下层不依赖上层
```

### 禁止的依赖

| 禁止行为 | 说明 |
|----------|------|
| Infrastructure 引用 Core | 基础层不感知 DataBus 等核心概念 |
| Core 引用 Service | DataBus 不包含业务逻辑 |
| Service 引用 UI | 服务层不感知 QML 窗口 |
| 跨层直接调用 | 所有跨层调用必须通过抽象接口 |
| 循环依赖 | 任何形式的 A→B 同时 B→A |

### 允许的依赖

| 允许行为 | 说明 |
|----------|------|
| UI → Service 接口 | 通过 `IService` 抽象基类 |
| Service → Core 接口 | 通过 `IDataBus` 抽象基类 |
| Core → Infrastructure 接口 | 通过 `ITransport` / `IConfig` 抽象基类 |
| Infrastructure → 第三方库 | spdlog, Qt Core, ASIO 等 |

---

## 技术选型与理由

| 技术 | 版本 | 理由 |
|------|------|------|
| **C++20** | C++20 | concepts 约束模板、ranges 管道操作、coroutines 异步 I/O、span 零拷贝 |
| **Qt 6.6** | 6.6+ LTS | 成熟跨平台 GUI，QML 声明式 UI，QtSerialPort/QCanBus 内置通信支持 |
| **CMake** | 3.27+ | 跨平台构建标准，Presets 简化多平台配置，FetchContent 管理依赖 |
| **spdlog** | 1.13+ | header-only，异步日志，fmt 格式化，性能优于大部分日志库 |
| **Protobuf** | 24.x | 跨语言序列化，二进制高效，向后兼容，适合协议帧定义 |
| **ASIO** | 1.28+ | 独立于 Boost 的 ASIO，proactor 模式，高效的 TCP/UDP 异步 I/O |
| **GoogleTest** | 1.14+ | C++ 单元测试事实标准，Mock 支持完善 |
| **vcpkg** | latest | 微软官方 C++ 包管理器，与 CMake 深度集成，Windows/Linux 统一 |

### 选型权衡

| 权衡点 | 选择 | 放弃方案 | 理由 |
|--------|------|----------|------|
| 序列化 | Protobuf | FlatBuffers, MessagePack | 生态成熟，工具链完善；零拷贝场景用 span 补充 |
| 通信 | Qt + ASIO 混合 | 纯 Qt / 纯 ASIO | 串口/CAN 用 Qt 原生；TCP 高性能场景用 ASIO |
| UI | QML | Qt Widgets, Web UI | 声明式 UI 开发效率高，动画/图表支持好 |
| 构建 | CMake | Meson, Bazel | Qt 官方支持 CMake，团队熟悉度高 |
| 日志 | spdlog | Boost.Log, glog | 异步性能好，header-only 集成简单 |

---

## 组件交互概览

### 典型上行数据流（MCU → UI）

```
MCU  ──[raw bytes]──►  SerialTransport
                           │
                           ▼
                      ModbusProtocol  ──[parsed frame]──►  DataBus
                                                                │
                           ┌────────────────────────────────────┤
                           ▼                                    ▼
                    DataMonitorService                   CurveEngineService
                           │                                    │
                           ▼                                    ▼
                    DashboardViewModel                   ScopeViewModel
                           │                                    │
                           ▼                                    ▼
                    DashboardView.qml                   ScopeView.qml
```

### 典型下行数据流（UI → MCU）

```
ConfigView.qml  ──[user input]──►  ConfigViewModel
                                        │
                                        ▼
                                  ParamManagerService
                                        │
                                        ▼
                                  CommandQueue (priority sorted)
                                        │
                                        ▼
                                  ModbusProtocol  ──[encoded frame]──►  SerialTransport  ──►  MCU
```

---

## 目录结构映射到层

```
src/
├── app/                          # 应用程序入口 (main.cpp)
│   ├── main.cpp
│   └── Application.cpp           # 初始化/关闭顺序编排
│
├── ui/                           # Layer 4: UI Layer
│   ├── qml/                      # QML View 文件
│   │   ├── DashboardView.qml
│   │   ├── ScopeView.qml
│   │   ├── ConfigView.qml
│   │   └── ...
│   └── viewmodels/               # C++ ViewModel
│       ├── DashboardViewModel.h/cpp
│       ├── ScopeViewModel.h/cpp
│       └── ...
│
├── service/                      # Layer 3: Service Layer
│   ├── DataMonitorService.h/cpp
│   ├── CurveEngineService.h/cpp
│   ├── ParamManagerService.h/cpp
│   ├── AutomationEngineService.h/cpp
│   └── ...
│
├── core/                         # Layer 2: Core Layer (DataBus)
│   ├── DataBus.h/cpp
│   ├── PubSubEngine.h/cpp
│   ├── RingBuffer.h/cpp
│   ├── TopicRegistry.h/cpp
│   ├── DataCache.h/cpp
│   ├── CommandQueue.h/cpp
│   └── DataPoint.h               # 核心数据结构
│
├── protocol/                     # Layer 2: Protocol Layer
│   ├── IProtocol.h               # 协议抽象接口
│   ├── ModbusProtocol.h/cpp
│   ├── CANopenProtocol.h/cpp
│   ├── EtherCATProtocol.h/cpp
│   └── ...
│
├── transport/                    # Layer 1: Transport Layer
│   ├── ITransport.h              # 传输抽象接口
│   ├── SerialTransport.h/cpp
│   ├── CANTransport.h/cpp
│   ├── TCPTransport.h/cpp
│   └── ...
│
└── infra/                        # Layer 1: 基础设施
    ├── ConfigManager.h/cpp
    ├── Logger.h/cpp
    ├── PluginLoader.h/cpp
    ├── CrashHandler.h/cpp
    └── ...
```

---

## 跨平台策略

### 平台差异隔离

```
┌─────────────────────────────────────────────────────────┐
│                   Platform Abstraction                    │
│                                                          │
│  ┌──────────────────────────────────────────────────┐   │
│  │              Platform Interface (IPlatform)       │   │
│  │  - getConfigPath() → std::filesystem::path        │   │
│  │  - getDataPath()   → std::filesystem::path        │   │
│  │  - getSerialPorts()→ std::vector<PortInfo>        │   │
│  │  - getCANDevices() → std::vector<CANInfo>         │   │
│  │  - crashHandler()  → void                         │   │
│  └───────────────────┬──────────────────────────────┘   │
│                      │                                   │
│       ┌──────────────┴──────────────┐                   │
│       ▼                              ▼                   │
│  ┌──────────┐                 ┌──────────┐              │
│  │ Windows  │                 │  Linux   │              │
│  │ Platform │                 │ Platform │              │
│  └──────────┘                 └──────────┘              │
└─────────────────────────────────────────────────────────┘
```

### 编译期平台选择

```cpp
// platform/IPlatform.h
class IPlatform {
public:
    virtual ~IPlatform() = default;
    virtual std::filesystem::path getConfigDir() const = 0;
    virtual std::filesystem::path getDataDir() const = 0;
    virtual std::vector<SerialPortInfo> enumerateSerialPorts() const = 0;
};

// platform/WindowsPlatform.h
#ifdef _WIN32
class WindowsPlatform : public IPlatform { /* ... */ };
#endif

// platform/LinuxPlatform.h
#ifdef __linux__
class LinuxPlatform : public IPlatform { /* ... */ };
#endif
```

### 跨平台注意事项

| 关注点 | 策略 |
|--------|------|
| **文件路径** | 统一使用 `std::filesystem::path`，禁止字符串拼接路径 |
| **行尾符** | `.gitattributes` 设置 `text=auto`，CMake 自动处理 |
| **编码** | 源码 UTF-8 BOM-less，运行时内部使用 UTF-8 |
| **动态库** | `.dll` (Win) / `.so` (Linux)，插件加载统一接口 |
| **串口设备名** | Windows: `COM1`；Linux: `/dev/ttyUSB0`；通过 `IPlatform` 抽象 |
| **CAN 接口** | Windows: PEAK/KSI；Linux: SocketCAN；通过 `ICANTransport` 抽象 |
| **信号处理** | SIGSEGV handler 跨平台，Windows 用 `SetUnhandledExceptionFilter` |
| **CI 矩阵** | GitHub Actions: `windows-2022` + `ubuntu-22.04` 双平台编译 |

---

## 后续实现注意事项

1. **分层执行的强制检查**：在 CMake 中添加 `check_include_dirs` 脚本，确保 `src/core/` 不 `#include` `src/service/` 等违规依赖
2. **接口稳定性**：`IProtocol`、`ITransport`、`IDataBus` 等核心接口应优先设计并冻结，后续修改需走 API 版本升级流程
3. **平台实现渐进式**：先完整实现 Windows 平台，再补充 Linux 平台；`IPlatform` 接口为 Linux 预留扩展点
4. **第三方库版本锁定**：vcpkg manifest 模式锁定所有依赖版本，避免 CI 环境不一致
5. **性能基线测试**：在 Infrastructure 层完成后立即建立性能基线（串口吞吐量、DataBus 吞吐量），后续每次架构变更都需回归性能测试
6. **日志级别规范**：trace/dev 用于调试，info 用于关键操作，warn 用于可恢复异常，error 用于不可恢复错误，critical 用于崩溃前
7. **配置热加载**：ConfigManager 支持 `SIGHUP` (Linux) / 自定义消息 (Windows) 触发配置重载，无需重启应用
8. **崩溃恢复**：CrashHandler 生成 minidump (Windows) / core dump (Linux)，并在下次启动时提示用户恢复上次会话