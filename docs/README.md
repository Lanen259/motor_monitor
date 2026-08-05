# Motor Studio — 工业级电机调试平台

> **文档版本**: v1.0  
> **最后更新**: 2026-08-05  
> **维护团队**: Motor Studio 架构组  

---

## 目标

Motor Studio 是一个面向工业现场的**电机驱动调试与监控平台**。它提供统一的通信接入、实时数据采集、参数配置、自动化测试和波形分析能力，支持多种电机控制协议（串口/CAN/EtherCAT），目标运行于 Windows 和 Linux 桌面环境。

### 核心目标

1. **统一接入**：屏蔽底层通信协议差异，提供一致的电机调试接口
2. **实时监控**：支持 100+ 变量、1kHz 采样率、亚毫秒级数据分发
3. **可扩展**：插件化协议栈，新协议零侵入接入
4. **跨平台**：Windows 10+ / Ubuntu 22.04+，一套代码，两套编译
5. **工业可靠**：7×24 稳定运行，完善的异常恢复与日志审计

---

## 技术栈

| 层次 | 技术选型 | 说明 |
|------|---------|------|
| **语言** | C++20 | 使用 concepts、ranges、coroutines、span |
| **UI 框架** | Qt 6.6+ | QML + C++ 混合，MVVM 架构 |
| **构建系统** | CMake 3.27+ | 跨平台构建，Presets 管理 |
| **包管理** | vcpkg / Conan | 第三方库版本锁定 |
| **通信库** | QtSerialPort, QCanBus, ASIO | 串口/CAN/TCP 统一封装 |
| **序列化** | Protobuf / flatbuffers | 协议帧编解码（按协议选择） |
| **日志** | spdlog | 异步日志，按日轮转 |
| **测试** | GoogleTest + QtTest | 单元/集成/UI 测试 |
| **CI/CD** | GitHub Actions | Windows + Linux 双平台矩阵构建 |

---

## 文档导航

### 架构设计

| 文档 | 说明 |
|------|------|
| [SystemArchitecture.md](architecture/SystemArchitecture.md) | 系统四层架构、依赖规则、技术选型 |
| [ModuleDesign.md](architecture/ModuleDesign.md) | 14 个核心模块目录、接口、依赖关系 |
| [ThreadModel.md](architecture/ThreadModel.md) | 5-6 线程模型、通信机制、安全策略 |
| [DataFlow.md](architecture/DataFlow.md) | 数据流架构、DataBus 发布订阅、性能目标 |

### 详细设计（待补充）

| 文档 | 说明 |
|------|------|
| `design/ProtocolLayer.md` | 协议解析层详细设计 |
| `design/TransportLayer.md` | 传输层抽象与实现 |
| `design/DataBus.md` | DataBus 内部机制、RingBuffer 实现 |
| `design/CurveRenderer.md` | 波形渲染引擎设计 |
| `design/AutomationEngine.md` | 自动化脚本引擎设计 |
| `design/PluginSystem.md` | 插件加载与生命周期 |

### 接口规范（待补充）

| 文档 | 说明 |
|------|------|
| `api/TransportInterface.md` | 传输层接口规范 |
| `api/ProtocolInterface.md` | 协议层接口规范 |
| `api/DataBusAPI.md` | DataBus 发布/订阅 API |
| `api/CommandAPI.md` | 命令下发接口规范 |

### 开发指南（待补充）

| 文档 | 说明 |
|------|------|
| `guides/SetupEnvironment.md` | 开发环境搭建 |
| `guides/CodingStandards.md` | C++20 编码规范 |
| `guides/AddNewProtocol.md` | 新增协议指南 |
| `guides/AddNewModule.md` | 新增模块指南 |
| `guides/TestingGuide.md` | 测试编写指南 |

---

## 架构概览

```
┌─────────────────────────────────────────────────────────────────────┐
│                         UI Layer (QML + C++)                         │
│  ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌───────────┐ │
│  │ Dashboard │ │  Scope   │ │  Config  │ │Automation│ │  LogView   │ │
│  └─────┬─────┘ └─────┬─────┘ └─────┬─────┘ └─────┬─────┘ └─────┬─────┘ │
├────────┼─────────────┼─────────────┼─────────────┼─────────────┼───────┤
│        │       Service Layer        │             │             │       │
│  ┌─────▼─────┐ ┌─────▼─────┐ ┌─────▼─────┐ ┌─────▼─────┐ ┌─────▼─────┐ │
│  │DataMonitor│ │CurveEngine│ │ParamMgr   │ │Automation │ │  Logger   │ │
│  │  Service  │ │  Service  │ │  Service  │ │  Service  │ │  Service  │ │
│  └─────┬─────┘ └─────┬─────┘ └─────┬─────┘ └─────┬─────┘ └─────┬─────┘ │
├────────┼─────────────┼─────────────┼─────────────┼─────────────┼───────┤
│        │        Core / Protocol Layer              │             │       │
│  ┌─────▼───────────────────────────────────────────▼─────────────▼─────┐ │
│  │                          DataBus                                     │ │
│  │  ┌────────────┐  ┌────────────┐  ┌────────────┐  ┌────────────┐    │ │
│  │  │  Pub/Sub   │  │ RingBuffer │  │  TopicMgr  │  │ DataCache  │    │ │
│  │  └────────────┘  └────────────┘  └────────────┘  └────────────┘    │ │
│  └──────────────────────────────┬──────────────────────────────────────┘ │
│  ┌──────────────────────────────┼──────────────────────────────────────┐ │
│  │                      Protocol Layer                                 │ │
│  │  ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌──────────┐               │ │
│  │  │  Modbus  │ │ CANopen  │ │EtherCAT  │ │  Custom  │               │ │
│  │  │ Protocol │ │ Protocol │ │ Protocol │ │ Protocol │               │ │
│  │  └────┬─────┘ └────┬─────┘ └────┬─────┘ └────┬─────┘               │ │
│  └───────┼────────────┼────────────┼────────────┼──────────────────────┘ │
├──────────┼────────────┼────────────┼────────────┼────────────────────────┤
│          │   Infrastructure / Transport Layer                            │
│  ┌───────▼────────────▼────────────▼────────────▼──────────────────────┐ │
│  │                       Transport Abstraction                          │ │
│  │  ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌──────────┐               │ │
│  │  │  Serial  │ │   CAN    │ │   TCP    │ │   USB    │               │ │
│  │  │ Transport│ │ Transport│ │ Transport│ │ Transport│               │ │
│  │  └──────────┘ └──────────┘ └──────────┘ └──────────┘               │ │
│  └─────────────────────────────────────────────────────────────────────┘ │
│  ┌─────────────────────────────────────────────────────────────────────┐ │
│  │                    Infrastructure Utilities                          │ │
│  │  ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌──────────┐               │ │
│  │  │  Config  │ │  Logger  │ │  Plugin  │ │  Crash   │               │ │
│  │  │  Manager │ │  (spdlog)│ │  Loader  │ │  Handler │               │ │
│  │  └──────────┘ └──────────┘ └──────────┘ └──────────┘               │ │
│  └─────────────────────────────────────────────────────────────────────┘ │
└─────────────────────────────────────────────────────────────────────────┘
```

---

## 10 项核心设计决策

| # | 决策 | 理由 |
|---|------|------|
| 1 | **四层架构（Infrastructure → Core → Service → UI）** | 单向依赖，层间松耦合；上层可替换，下层稳定 |
| 2 | **DataBus 采用发布/订阅 + 广播语义** | 解耦数据生产者与消费者；支持多订阅者同时接收同一份数据 |
| 3 | **Topic 使用整数 ID 而非字符串** | 性能优先：整数比较 O(1)，字符串哈希有碰撞风险和额外开销 |
| 4 | **RingBuffer 无锁设计 + 零拷贝** | 满足 1kHz/100+ 变量的吞吐要求，避免锁竞争 |
| 5 | **通信线程与 DataBus 线程分离** | 隔离 I/O 阻塞与数据分发，避免串口抖动影响 UI |
| 6 | **协议层插件化** | 新增协议无需修改 DataBus 和上层代码，符合开闭原则 |
| 7 | **QML + C++ MVVM** | QML 声明式 UI 开发效率高；C++ 承担计算密集型逻辑 |
| 8 | **日志线程独立写盘** | 避免日志 I/O 阻塞业务线程，spdlog async 模式天然支持 |
| 9 | **WorkerPool 共享线程池** | 曲线渲染、自动化脚本等 CPU 密集任务复用线程，避免线程膨胀 |
| 10 | **单实例 + 显式生命周期管理** | 核心模块（DataBus、ConfigManager）采用单例，明确 init/shutdown 调用顺序 |

---

## 架构审查发现与修复

> 以下为 v1.0 架构审查中发现的 10 个问题及其修复方案，已融入当前设计。

| # | 问题 | 严重程度 | 修复方案 |
|---|------|----------|----------|
| 1 | **DataBus 线程无 Watchdog 监控** | 高 | 新增独立 Watchdog 线程，心跳超时 3s 触发紧急日志 + 尝试重启 |
| 2 | **线程关闭顺序未文档化** | 高 | 明确关闭顺序：UI → Service → DataBus → Protocol → Transport → Logger；每步有超时 |
| 3 | **RingBuffer 无溢出保护** | 高 | 新增 `overrun_policy`：drop_oldest / drop_newest / block；默认 drop_oldest + 溢出计数 |
| 4 | **Topic 注册无冲突检测** | 中 | 构建时通过 CMake 扫描所有 Topic 注册点，生成冲突报告；运行时 assert |
| 5 | **命令队列无优先级** | 中 | CommandQueue 增加优先级通道：紧急停止(0) > 配置(1) > 查询(2) |
| 6 | **协议层无超时/重试机制** | 中 | 每协议实现统一的 `timeout_ms` + `retry_count` 配置，Transport 层提供超时回调 |
| 7 | **DataCache 无过期策略** | 中 | 每个 DataPoint 携带 timestamp，订阅者可配置 `max_age_ms`，超时数据标记为 stale |
| 8 | **插件加载无版本校验** | 低 | 插件 manifest 包含 `api_version`，加载时与主程序版本比对，不兼容则拒绝加载 |
| 9 | **跨平台文件路径未统一** | 低 | 统一使用 `std::filesystem::path`，禁止硬编码 `/` 或 `\` |
| 10 | **日志敏感信息泄露风险** | 低 | 日志增加 `[REDACTED]` 标记，参数写操作日志自动脱敏（只记录变量 ID，不记录值） |

---

## 如何贡献文档

### 文档规范

1. **格式**：所有设计文档使用 Markdown，遵循本 README 的章节结构
2. **命名**：PascalCase 命名，如 `SystemArchitecture.md`
3. **章节**：每个设计文档至少包含：目标、设计原则、类/模块关系、数据流、API 接口规划、后续实现注意事项
4. **图表**：使用 ASCII Art 或 Mermaid 绘制架构图

### 提交流程

```
1. 在 docs/ 下创建或修改文档
2. 更新 docs/README.md 中的导航链接
3. 提交 PR，标题格式: docs: <简短描述>
4. 至少一位架构组成员 Review
5. 合并后同步更新相关代码注释
```

### 文档维护责任

| 文档分区 | 负责人 | 说明 |
|----------|--------|------|
| `architecture/` | 架构组 | 系统级架构决策 |
| `design/` | 各模块负责人 | 模块详细设计 |
| `api/` | 接口定义组 | 跨模块接口 |
| `guides/` | 全员 | 开发指南与最佳实践 |

---

## 目录结构映射

```
Motor_Monitor/
├── docs/                          # ← 本文档根目录
│   ├── README.md
│   ├── architecture/
│   │   ├── SystemArchitecture.md
│   │   ├── ModuleDesign.md
│   │   ├── ThreadModel.md
│   │   └── DataFlow.md
│   ├── design/                    # 详细设计（待补充）
│   ├── api/                       # 接口规范（待补充）
│   └── guides/                    # 开发指南（待补充）
├── src/
│   ├── app/                       # 应用程序入口
│   ├── ui/                        # UI 层（QML + C++ ViewModel）
│   ├── service/                   # 服务层
│   ├── core/                      # 核心层（DataBus）
│   ├── protocol/                  # 协议层
│   ├── transport/                 # 传输层
│   └── infra/                     # 基础设施层
├── tests/
├── cmake/
├── scripts/
├── CMakeLists.txt
└── README.md
```