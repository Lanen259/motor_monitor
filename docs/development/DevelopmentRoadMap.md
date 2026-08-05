# 开发路线图 (Development Roadmap)

## 目标

制定 Motor Monitor 上位机三年开发路线图，确保项目有序推进，每个阶段产出可运行的演示系统。通过垂直切片策略和基础设施优先原则，降低技术风险，逐步交付用户价值。

---

## 设计原则

1. **基础设施优先**：Logger → Thread → Protocol → Data → UI，底层不稳绝不建上层。
2. **垂直切片**：每个阶段产出可运行的端到端演示，而非孤立的功能模块。
3. **渐进增强**：MVP 先满足核心场景，后续迭代持续增强非核心能力。
4. **风险前置**：在早期阶段识别和解决最高风险的技术挑战。
5. **可演示可度量**：每个里程碑有明确的验收标准和演示目标。

---

## 类/模块关系

### 模块依赖层级（基础设施优先）

```
                    ┌─────────────────────────────────────┐
                    │               UI Layer               │  Phase 1 M3
                    │  MainWindow, WaveformPanel,         │
                    │  ParamConfigPanel, StatusBar        │
                    ├─────────────────────────────────────┤
                    │             Data Layer               │  Phase 1 M3
                    │  DataBus, ParameterModel,           │
                    │  WaveformData, DataExporter         │
                    ├─────────────────────────────────────┤
                    │           Protocol Layer             │  Phase 1 M2
                    │  ProtocolBase, ModbusRTU,           │
                    │  ProtocolFactory                    │
                    ├─────────────────────────────────────┤
                    │            Device Layer              │  Phase 1 M2
                    │  SerialPortManager, DeviceManager,  │
                    │  MotorController                    │
                    ├─────────────────────────────────────┤
                    │            Core Layer                │  Phase 1 M1
                    │  ThreadPool, EventBus,              │
                    │  ConfigManager, PluginManager(stub) │
                    ├─────────────────────────────────────┤
                    │           Logger Layer               │  Phase 1 M1
                    │  LogManager, ConsoleSink, FileSink  │
                    └─────────────────────────────────────┘

Phase 2 扩展:
  ├─ Protocol Layer 新增: CANDriver, TCPDriver, UDPDriver, CANopenProtocol
  ├─ Device Layer 新增: MultiDeviceManager, FirmwareUpdater, AutoDetector
  ├─ Data Layer 新增: DataRecorder, DataReplayer, DataIndexer
  ├─ Core Layer 新增: LuaEngine, AutomationEngine
  └─ UI Layer 新增: PIDTuningPanel, AutomationPanel, MultiDevicePanel

Phase 3 扩展:
  ├─ Core Layer 新增: PluginManager(完整), PythonEngine, AIModelManager
  ├─ Data Layer 新增: CloudSync, RemoteDataBus
  ├─ UI Layer 新增: DashboardEditor, WebDashboard
  └─ 新增: Cloud Layer (独立模块), OpenAPI Layer (独立模块)
```

### 阶段间依赖关系

```
Phase 1 ──────▶ Phase 2 ──────▶ Phase 3
   │                │                │
   │ 提供:          │ 提供:          │ 提供:
   │ - 稳定 API     │ - 扩展 API     │ - 平台 API
   │ - 核心框架     │ - 插件接口     │ - 云接口
   │ - 基础协议     │ - 脚本引擎     │ - AI 接口
   │                │                │
   ▼                ▼                ▼
 Phase 2 依赖      Phase 3 依赖     最终交付
 Phase 1 全部      Phase 1+2 全部
 功能              功能
```

---

## 数据流

### 垂直切片数据流（以 Phase 1 为例）

```
用户操作
  │
  ▼
UI Layer (MainWindow)
  │ 用户选择串口、配置参数
  ▼
Device Layer (SerialPortManager)
  │ 打开串口、建立连接
  ▼
Protocol Layer (ModbusRTU)
  │ 发送读寄存器命令
  │ 接收响应 → 解析
  ▼
Device Layer (MotorController)
  │ 处理数据 → 更新状态
  ▼
Data Layer (DataBus)
  │ 发布 MotorData 事件
  ▼
UI Layer (WaveformPanel)
  │ 订阅 MotorData → 渲染波形
  ▼
用户看到实时波形
  │
  ▼
Data Layer (DataExporter)
  │ 导出 CSV/PNG
  ▼
文件系统
```

### Phase 间数据流演进

```
Phase 1: 单设备串口数据流
  SerialPort → ModbusRTU → MotorController → DataBus → WaveformPanel

Phase 2: 多设备多协议数据流
  SerialPort ─┐
  CANDriver  ─┤→ MultiProtocolRouter → MultiDeviceManager → DataBus
  TCPDriver  ─┘       │                                        │
                      │                          ┌─────────────┤
                      │                          ▼             ▼
                      │                    DataRecorder  WaveformPanel
                      │                          │        (multi-device)
                      │                          ▼
                      │                    DataReplayer
                      │                          │
                      └──────────────────────────┘
                             (录制时保存原始协议数据)

Phase 3: 平台化数据流
  插件协议 ─┐
  REST API ─┤→ PluginManager → DataBus → UI / Web Dashboard
  Python   ─┘                             │
                                          ▼
                                     CloudSync → 云端存储 → Web 远程监控
```

### 风险前置数据流

```
Phase 1 早期 (M1-M2):
  高风险技术验证 ──▶ 实时波形渲染 (OpenGL 原型)
                 ──▶ 串口通信稳定性 (压力测试)
                 ──▶ 日志系统性能 (基准测试)

Phase 2 早期 (M4-M5):
  高风险技术验证 ──▶ CAN 通信兼容性 (多适配器测试)
                 ──▶ 多设备同步精度 (时间戳对齐测试)
                 ──▶ Lua 嵌入安全性 (沙箱测试)

Phase 3 早期 (M10-M11):
  高风险技术验证 ──▶ 插件 C ABI 稳定性 (跨编译器测试)
                 ──▶ Python 嵌入性能 (GIL 影响评估)
                 ──▶ AI 模型推理延迟 (ONNX Runtime 基准)
```

---

## API 接口规划

### Phase 1 核心 API

```cpp
// 设备管理 API
class IDeviceManager {
public:
    virtual Result<void> connect(const ConnectionConfig& config) = 0;
    virtual Result<void> disconnect() = 0;
    virtual bool isConnected() const = 0;
    virtual Result<DeviceInfo> getDeviceInfo() = 0;
};

// 协议 API
class IProtocol {
public:
    virtual Result<QByteArray> readRegister(uint16_t addr, uint16_t count) = 0;
    virtual Result<void> writeRegister(uint16_t addr, uint16_t value) = 0;
    virtual Result<ProtocolInfo> getProtocolInfo() const = 0;
};

// 数据总线 API
class IDataBus {
public:
    virtual void publish(const QString& topic, const QVariant& data) = 0;
    virtual SubscriptionHandle subscribe(const QString& topic,
                                          std::function<void(const QVariant&)> callback) = 0;
    virtual void unsubscribe(SubscriptionHandle handle) = 0;
};
```

### Phase 2 扩展 API

```cpp
// 多设备管理
class IMultiDeviceManager : public IDeviceManager {
public:
    virtual Result<void> connectAll() = 0;
    virtual Result<void> disconnectAll() = 0;
    virtual std::vector<DeviceId> getConnectedDevices() const = 0;
    virtual Result<void> syncTrigger(const std::vector<DeviceId>& devices) = 0;
};

// 数据录制与回放
class IDataRecorder {
public:
    virtual Result<void> startRecording(const RecordConfig& config) = 0;
    virtual Result<void> stopRecording() = 0;
    virtual RecordState getState() const = 0;
};

class IDataReplayer {
public:
    virtual Result<void> load(const QString& filePath) = 0;
    virtual Result<void> play(double speed = 1.0) = 0;
    virtual Result<void> pause() = 0;
    virtual Result<void> seek(std::chrono::milliseconds position) = 0;
};

// Lua 脚本 API
class ILuaEngine {
public:
    virtual Result<void> executeScript(const QString& script) = 0;
    virtual Result<void> executeFile(const QString& filePath) = 0;
    virtual void registerAPI(const QString& name, const LuaAPI& api) = 0;
};
```

### Phase 3 平台 API

```cpp
// 插件接口
class IPlugin {
public:
    virtual const PluginManifest* getManifest() const = 0;
    virtual Result<void> initialize() = 0;
    virtual void shutdown() = 0;
    virtual void* getInterface(PluginType type) = 0;
};

// Python 脚本 API
class IPythonEngine {
public:
    virtual Result<void> executeScript(const QString& script) = 0;
    virtual Result<QVariant> evaluate(const QString& expression) = 0;
    virtual void bindObject(const QString& name, QObject* object) = 0;
};

// 云同步 API
class ICloudSync {
public:
    virtual Result<void> upload(const DataSet& data) = 0;
    virtual Result<DataSet> download(const TimeRange& range) = 0;
    virtual Result<void> syncConfig(const DeviceConfig& config) = 0;
};

// Open REST API (HTTP)
// GET    /api/v1/devices              - 设备列表
// GET    /api/v1/devices/{id}         - 设备详情
// GET    /api/v1/devices/{id}/params  - 设备参数
// PUT    /api/v1/devices/{id}/params  - 修改参数
// GET    /api/v1/devices/{id}/data    - 实时数据
// WS     /api/v1/ws/live              - WebSocket 实时推送
// POST   /api/v1/recordings           - 开始录制
// GET    /api/v1/recordings           - 录制列表
```

### API 版本演进策略

| 阶段 | API 版本 | 兼容策略 |
|------|----------|----------|
| Phase 1 | v1.0.x | 初始 API，可能不稳定 |
| Phase 2 | v1.x.0 | 向后兼容 Phase 1，新增 API 标记为 `@since 1.x` |
| Phase 3 | v2.0.0 | 可能引入 Breaking Change，提供 v1→v2 迁移指南 |

---

## 总览

```
┌────────────────────────────────────────────────────────────────────────────┐
│                          Motor Monitor 三年路线图                            │
│                                                                            │
│  Phase 1 (MVP)          Phase 2 (Enhancement)       Phase 3 (Maturity)     │
│  0 ─── 6 个月           6 ─── 18 个月                18 ─── 36 个月         │
│                                                                            │
│  ┌──────────────┐      ┌──────────────────┐      ┌──────────────────────┐ │
│  │ 核心框架      │      │ CAN / TCP / UDP   │      │ 插件生态             │ │
│  │ 串口通信      │      │ 多设备管理        │      │ 跨平台              │ │
│  │ 通信协议      │      │ 数据录制/回放     │      │ Python 脚本         │ │
│  │ 参数配置      │      │ 固件升级          │      │ AI 辅助调参         │ │
│  │ 实时波形      │      │ 电机自动检测      │      │ 云集成              │ │
│  │ 日志系统      │      │ PID 整定          │      │ Open API            │ │
│  │ 线程模型      │      │ 自动化测试        │      │ 自定义仪表盘        │ │
│  │              │      │ Lua 脚本          │      │ 国际化              │ │
│  └──────────────┘      └──────────────────┘      └──────────────────────┘ │
│                                                                            │
│  目标: 单电机串口      目标: 多电机多协议        目标: 平台化生态           │
│        基础监控               自动化测试                 行业解决方案       │
└────────────────────────────────────────────────────────────────────────────┘
```

---

## Phase 1: MVP（0-6 个月）

### 目标
构建上位机最小可行产品，支持单电机通过串口连接、参数配置、实时波形显示和基本日志。

### 里程碑

#### M1: 基础设施搭建（第 1-2 个月）

| 任务 | 描述 | 验收标准 |
|------|------|----------|
| 项目骨架 | CMake 构建系统、目录结构、CI 配置 | 跨平台编译通过 |
| 日志系统 | LogManager + 10 类别 + Console/File Sink | 所有模块可输出日志，< 1μs 延迟 |
| 线程模型 | 主线程/IO 线程/工作线程池 | 线程间通信正常，无死锁 |
| 编码规范 | clang-format / clang-tidy 配置 | CI 通过检查 |
| 基础 UI 框架 | QMainWindow + 菜单栏 + 状态栏 + 主题 | 窗口正常显示，可切换主题 |

**可演示产出**：带日志输出的空白应用窗口

#### M2: 串口通信与协议（第 3-4 个月）

| 任务 | 描述 | 验收标准 |
|------|------|----------|
| 串口管理 | 枚举/打开/关闭/配置串口 | 正确识别和操作串口设备 |
| 协议框架 | 抽象协议接口 + 协议工厂 | 可注册和切换协议 |
| Modbus RTU | 读寄存器/写寄存器/异常处理 | 与真实电机通信成功 |
| 协议日志 | TX/RX 报文日志，十六进制显示 | 每条报文可追溯 |
| 串口面板 UI | 串口选择/波特率/数据位/停止位/校验 | 参数配置正确生效 |

**可演示产出**：通过串口连接电机，发送 Modbus 命令并显示响应

#### M3: 参数配置与实时波形（第 5-6 个月）

| 任务 | 描述 | 验收标准 |
|------|------|----------|
| 参数模型 | 参数树结构（分组/只读/读写/范围限制） | 参数正确加载和校验 |
| 参数配置面板 | 树形控件 + 编辑/导入/导出 | 参数可编辑和持久化 |
| 实时数据采集 | 定时轮询电机参数 | 采集间隔 10-100ms 可配 |
| 波形显示 | 多通道波形 + 缩放/平移/暂停 | 60fps 流畅渲染 |
| 波形导出 | 导出为 CSV/PNG | 数据正确导出 |
| 系统托盘 | 最小化到托盘，后台运行 | 托盘菜单正常 |

**可演示产出**：连接电机 → 配置参数 → 查看实时波形 → 导出数据

### Phase 1 交付物

- [ ] 可执行安装包（Windows）
- [ ] 用户手册（基本操作）
- [ ] 开发文档（架构、接口、编码规范）
- [ ] 源代码仓库（含 CI）

---

## Phase 2: 增强（6-18 个月）

### 目标
在 MVP 基础上扩展通信协议、多设备管理、数据录制/回放、固件升级、自动化测试和脚本能力，将上位机从"监视工具"升级为"调试平台"。

### 里程碑

#### M4: 多协议支持（第 7-9 个月）

| 任务 | 描述 | 验收标准 |
|------|------|----------|
| CAN 通信 | CAN 适配器枚举 + 报文收发 + 滤波 | 通过 CAN 连接电机并通信 |
| TCP/UDP 通信 | TCP 客户端/服务端 + UDP 单播/广播 | 通过网络连接电机 |
| 协议扩展 | CANopen 协议支持 | 读写 CANopen 对象字典 |
| 协议切换 | 运行时切换通信协议 | 无缝切换不丢数据 |
| 连接管理 | 统一连接状态机 | 断线自动重连 |

**可演示产出**：通过串口/CAN/网络连接同一电机，协议切换无缝

#### M5: 多设备管理（第 10-12 个月）

| 任务 | 描述 | 验收标准 |
|------|------|----------|
| 设备管理 | 多设备发现/注册/分组 | 同时管理 10+ 设备 |
| 多设备波形 | 多设备波形叠加显示 | 不同颜色区分，独立开关 |
| 设备拓扑 | 主从设备关系可视化 | 拓扑图正确显示 |
| 同步采集 | 多设备同步触发采集 | 时间戳对齐 |
| 设备模板 | 设备配置模板 | 快速配置同类设备 |

**可演示产出**：同时连接 3 台电机，各自独立控制和波形显示

#### M6: 数据录制与回放（第 10-12 个月）

| 任务 | 描述 | 验收标准 |
|------|------|----------|
| 数据录制 | 录制所有参数到二进制文件 | 录制 1 小时不丢数据 |
| 数据回放 | 回放录制的数据 | 波形实时重现，支持快进/慢放 |
| 录制触发 | 手动/定时/条件触发录制 | 条件触发准确 |
| 数据索引 | 时间索引 + 参数索引 | 快速定位任意时刻数据 |
| 录制管理 | 录制列表/删除/导出 | 管理界面完整 |

**可演示产出**：录制 10 分钟运行数据 → 回放波形 → 导出分析

#### M7: 固件升级与电机检测（第 13-15 个月）

| 任务 | 描述 | 验收标准 |
|------|------|----------|
| 固件升级 | 通过串口/CAN 升级电机固件 | 升级成功，校验通过 |
| 升级保护 | 断线恢复、版本校验、回滚 | 升级失败可恢复 |
| 自动检测 | 自动识别电机型号和参数 | 识别准确率 > 95% |
| 参数自动配置 | 根据检测结果自动配置参数 | 配置正确可用 |
| 批量升级 | 同时升级多台电机 | 进度显示，失败隔离 |

**可演示产出**：连接未知电机 → 自动识别型号 → 升级固件 → 验证

#### M8: PID 整定与自动化测试（第 13-15 个月）

| 任务 | 描述 | 验收标准 |
|------|------|----------|
| PID 监视 | 实时显示 PID 参数和响应 | 参数和波形同时显示 |
| PID 整定 | 自动整定（Ziegler-Nichols / 继电反馈） | 整定结果稳定 |
| 手动整定辅助 | 阶跃响应分析 / 频率响应分析 | 分析结果可视化 |
| 测试框架 | 测试步骤定义/执行/报告 | 支持顺序/条件/循环 |
| 测试步骤库 | 预设常用测试步骤 | 开环/闭环/负载测试 |
| 测试报告 | 自动生成测试报告 (PDF) | 报告包含波形和数据分析 |

**可演示产出**：对电机进行 PID 自动整定 → 运行自动化测试 → 生成报告

#### M9: Lua 脚本（第 16-18 个月）

| 任务 | 描述 | 验收标准 |
|------|------|----------|
| Lua 集成 | 嵌入 Lua 5.4 解释器 | 脚本正确执行 |
| API 绑定 | 电机控制/数据读取/波形 API | API 覆盖核心功能 |
| 脚本编辑器 | 语法高亮 + 自动补全 + 调试 | 编辑器功能完整 |
| 脚本示例 | 常用脚本模板 | 用户可基于模板修改 |
| 安全沙箱 | 限制脚本系统调用和资源 | 脚本不会崩溃宿主 |

**可演示产出**：编写 Lua 脚本 → 自动执行测试序列 → 输出结果

### Phase 2 交付物

- [ ] 多协议连接的完整上位机
- [ ] 数据录制/回放功能
- [ ] 固件升级工具
- [ ] 自动化测试框架
- [ ] 开发者文档（Lua API）
- [ ] 用户手册（完整版）

---

## Phase 3: 成熟（18-36 个月）

### 目标
将上位机从"调试平台"升级为"行业解决方案平台"，通过插件生态、跨平台支持、脚本扩展、AI 辅助和云集成，覆盖更广泛的用户场景。

### 里程碑

#### M10: 插件生态（第 19-22 个月）

| 任务 | 描述 | 验收标准 |
|------|------|----------|
| 插件系统 | 完整的插件加载/管理/安全机制 | 详见 PluginArchitecture.md |
| 插件 SDK | 插件开发工具包和文档 | 第三方可独立开发插件 |
| 插件市场 | 在线浏览/安装/更新插件 | 市场功能完整 |
| 示例插件 | 每种类型至少 2 个示例插件 | 覆盖所有插件类型 |
| 开发者社区 | 论坛/文档/示例代码 | 社区活跃 |

**可演示产出**：安装第三方协议插件 → 通过新协议连接电机 → 功能正常

#### M11: 跨平台（第 19-22 个月）

| 任务 | 描述 | 验收标准 |
|------|------|----------|
| Linux 移植 | 适配 Linux 串口/CAN/UI | 功能与 Windows 一致 |
| macOS 移植 | 适配 macOS 串口/UI | 核心功能可用 |
| 安装包 | Linux AppImage/deb, macOS DMG | 一键安装 |
| 跨平台测试 | 自动化跨平台测试 | CI 覆盖三平台 |

**可演示产出**：同一代码库在 Windows/Linux/macOS 上编译运行

#### M12: Python 脚本（第 23-25 个月）

| 任务 | 描述 | 验收标准 |
|------|------|----------|
| Python 嵌入 | 嵌入 CPython 3.12+ | Python 脚本正确执行 |
| 绑定生成 | 自动生成 Python 绑定 | API 覆盖全部核心功能 |
| 科学计算集成 | 集成 numpy/scipy/matplotlib | 数据分析脚本正常运行 |
| Jupyter 集成 | 在 Jupyter Notebook 中控制电机 | 交互式数据探索 |
| Python 脚本库 | 常用分析脚本模板 | 用户可扩展 |

**可演示产出**：Python 脚本读取数据 → numpy 分析 → matplotlib 绘图

#### M13: AI 辅助调参（第 23-25 个月）

| 任务 | 描述 | 验收标准 |
|------|------|----------|
| 数据采集 | 采集大量运行数据作为训练集 | 数据量和质量达标 |
| 模型训练 | 训练 PID 参数优化模型 | 优化效果优于传统方法 |
| AI 建议 | 根据当前状态推荐参数 | 建议合理且可解释 |
| 异常检测 | 检测运行异常并预警 | 误报率 < 5% |
| 预测维护 | 预测电机维护时间 | 预测准确率 > 80% |

**可演示产出**：AI 分析运行数据 → 推荐参数优化 → 一键应用

#### M14: 云集成（第 26-30 个月）

| 任务 | 描述 | 验收标准 |
|------|------|----------|
| 云平台 | 数据上传/存储/查询 | 数据安全传输和存储 |
| 远程监控 | Web 端实时监控 | 延迟 < 2s |
| 设备管理 | 云端设备注册/分组/配置 | 管理界面完整 |
| 数据分析 | 云端历史数据分析 | 趋势图/报表/对比 |
| 告警推送 | 邮件/短信/App 推送 | 告警及时送达 |
| 多租户 | 租户隔离 | 数据完全隔离 |

**可演示产出**：多台电机数据上传云端 → Web 端实时监控 → 异常告警

#### M15: Open API 与自定义仪表盘（第 26-30 个月）

| 任务 | 描述 | 验收标准 |
|------|------|----------|
| REST API | 完整的 RESTful API | API 文档完整 |
| WebSocket API | 实时数据推送 | 延迟 < 500ms |
| SDK | Python/JavaScript/Java SDK | SDK 可用 |
| 自定义仪表盘 | 拖拽式仪表盘编辑器 | 所见即所得 |
| 仪表盘市场 | 分享/下载仪表盘模板 | 社区活跃 |

**可演示产出**：第三方应用通过 REST API 获取电机数据 → 自定义仪表盘展示

#### M16: 国际化（第 31-36 个月）

| 任务 | 描述 | 验收标准 |
|------|------|----------|
| i18n 框架 | Qt Linguist 集成 | 所有文本可翻译 |
| 中文 | 简体中文（默认） | 完整翻译 |
| 英文 | 英文翻译 | 完整翻译 |
| 日文 | 日文翻译 | 核心功能翻译 |
| 翻译工作流 | 翻译平台集成（Crowdin/Weblate） | 社区贡献翻译 |
| RTL 支持 | 阿拉伯语等 RTL 语言布局 | UI 正确渲染 |

**可演示产出**：切换语言 → 全部 UI 文本更新 → 无布局问题

### Phase 3 交付物

- [ ] 跨平台安装包（Windows/Linux/macOS）
- [ ] 插件 SDK 和文档
- [ ] Python SDK 和文档
- [ ] REST API 文档
- [ ] 云平台（Web 端）
- [ ] 多语言版本
- [ ] 完整用户手册和开发者文档

---

## 垂直切片策略

每个阶段必须产出可运行的端到端演示，而非孤立的功能模块。以下是各阶段的垂直切片：

```
Phase 1 垂直切片:
  Logger ──▶ Thread ──▶ Protocol ──▶ Data ──▶ UI
  ┌──────────────────────────────────────────────┐
  │ 用户打开应用 → 选择串口 → 连接电机 → 看到    │
  │ 实时波形 → 修改参数 → 导出数据               │
  └──────────────────────────────────────────────┘

Phase 2 垂直切片:
  Phase 1 功能 + 多协议 + 多设备 + 录制回放 + 自动化
  ┌──────────────────────────────────────────────┐
  │ 用户连接多台电机 → 多种协议 → 录制测试过程   │
  │ → 回放分析 → 固件升级 → 自动化测试 → 报告    │
  └──────────────────────────────────────────────┘

Phase 3 垂直切片:
  Phase 2 功能 + 插件 + 跨平台 + AI + 云
  ┌──────────────────────────────────────────────┐
  │ 用户安装第三方插件 → Python 脚本分析 → AI    │
  │ 推荐参数 → 上传云端 → Web 远程监控 → 告警     │
  └──────────────────────────────────────────────┘
```

---

## 基础设施优先原则

```
依赖顺序（自底向上）：

                    ┌─────────────────────┐
                    │        UI           │  ← 最后构建
                    │  (Panels, Widgets)  │
                    ├─────────────────────┤
                    │       Data          │  ← 数据模型
                    │  (DataBus, Models)  │
                    ├─────────────────────┤
                    │     Protocol        │  ← 通信协议
                    │  (Modbus, CANopen)  │
                    ├─────────────────────┤
                    │      Thread         │  ← 线程模型
                    │  (Worker Pool)      │
                    ├─────────────────────┤
                    │      Logger         │  ← 最先构建
                    │  (LogManager)       │
                    └─────────────────────┘

每一层必须在上一层开始前通过充分的单元测试和集成测试。
```

---

## 风险分析与缓解措施

### 技术风险

| 风险 | 概率 | 影响 | 缓解措施 |
|------|------|------|----------|
| 实时波形渲染性能不足 | 中 | 高 | Phase 1 早期原型验证，使用 OpenGL 加速 |
| 跨平台兼容性问题 | 高 | 中 | Phase 3 才做跨平台，降低优先级；Phase 1 用条件编译隔离平台代码 |
| 插件 C ABI 稳定性 | 中 | 高 | Phase 1 预留接口设计，Phase 3 正式实现；充分版本管理 |
| 多设备同步精度不足 | 中 | 中 | Phase 2 早期原型验证同步方案 |
| CAN 适配器兼容性 | 高 | 中 | 支持主流适配器（PCAN, Kvaser, SocketCAN），抽象适配器层 |
| AI 模型效果不佳 | 高 | 低 | Phase 3 才引入，有充足时间迭代；先做基于规则的传统方法 |
| 第三方库依赖冲突 | 中 | 中 | 使用 Conan/vcpkg 管理依赖，静态链接核心库 |

### 进度风险

| 风险 | 概率 | 影响 | 缓解措施 |
|------|------|------|----------|
| 需求变更 | 高 | 中 | 架构设计保持灵活性，插件系统支持扩展 |
| 人员变动 | 中 | 高 | 完善文档，代码风格统一，降低交接成本 |
| 硬件获取延迟 | 中 | 中 | 开发模拟器，不依赖真实硬件进行开发 |
| 第三方 API 变更 | 低 | 中 | 封装第三方依赖，隔离变更影响 |

### 质量风险

| 风险 | 概率 | 影响 | 缓解措施 |
|------|------|------|----------|
| 测试覆盖不足 | 中 | 高 | Phase 1 建立 CI 体系，覆盖率门禁 > 80% |
| 内存泄漏 | 中 | 中 | RAII 原则，AddressSanitizer 持续检测 |
| 线程安全问题 | 中 | 高 | 线程模型集中设计，ThreadSanitizer 持续检测 |
| 协议兼容性问题 | 高 | 中 | 与多种电机进行兼容性测试 |

---

## 资源需求估算

### 团队配置

| 角色 | Phase 1 | Phase 2 | Phase 3 |
|------|---------|---------|---------|
| 架构师/技术负责人 | 1 | 1 | 1 |
| C++/Qt 开发 | 2 | 3-4 | 4-5 |
| 嵌入式/协议开发 | 1 | 2 | 2 |
| UI/UX 设计 | 0.5 | 1 | 1 |
| 测试工程师 | 0.5 | 1 | 1-2 |
| 技术文档 | 0.5 | 0.5 | 1 |
| **合计** | **5.5** | **8.5-9.5** | **10.5-12.5** |

### 硬件资源

- 开发用电机：至少 2 台（不同型号）
- CAN 适配器：PCAN-USB 或兼容型号
- 串口工具：USB 转 RS485
- 测试用电机：覆盖主流型号

---

## 后续实现注意事项

1. **路线图迭代**：本路线图每季度评审一次，根据实际进展和用户反馈调整优先级和里程碑。

2. **技术预研**：Phase 2 和 Phase 3 的关键技术（Lua 嵌入、Python 嵌入、AI 模型）应在 Phase 1 期间进行预研和可行性验证。

3. **用户反馈闭环**：Phase 1 交付后立即收集用户反馈，优先解决用户痛点。

4. **向后兼容**：Phase 2 和 Phase 3 的 API 变更应保持向后兼容，或提供明确的迁移指南。

5. **文档先行**：每个模块的 API 文档在编码前确定，避免接口频繁变更。

6. **原型验证**：高风险任务（实时波形、多设备同步、AI 调参）先做原型验证技术可行性，再正式开发。

7. **开源策略**：Phase 2 后期考虑开源核心框架，通过插件市场建立商业模式。

8. **竞品跟踪**：持续跟踪同行业上位机产品（如 NI MAX、Kollmorgen WorkBench、Elmo Application Studio），保持竞争力。

9. **技术债务管理**：每个 Phase 结束前安排 2-4 周的"偿还期"，集中处理技术债务。

10. **安全审计**：Phase 3 云集成前进行安全审计，确保数据传输和存储安全。