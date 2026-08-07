# Upgrade_Report.md — Motor Automation 工业级升级分析报告

> **报告类型**: Phase 1 现状分析（只分析，不改码）
> **日期**: 2026-08-06
> **分析人**: Conductor / Chief Architect
> **依据**: `motor_antomation_Software_Design_Document.md`（v1.3，13章+附录）、`docs/` 14 篇设计文档、`motor_antomation/src/` 全部 79 个源文件、`.agent/TASK_BOARD.md`、git log
> **范围**: 将当前"可运行的工程 Demo"升级为"可用于研发、调试、生产测试的工业级电机自动化平台"

---

## 0. 结论速览

| 维度 | 现状 | 结论 |
|------|------|------|
| 构建系统 | **Qt 5.14.2 MinGW 32-bit / qmake / C++17**，与设计文档（Qt6/C++20/CMake）不符；CMake 无顶层文件，**不可构建** | 🔴 高优先级修复 |
| 数据管道 | `SerialTransport→VofaParser→ChannelManager→UI` 直连（MainWindow 内），**DataBus/CurveEngine/DataManager 存在但未接入应用** | 🔴 高优先级修复 |
| 动态通道 | 通道数/名称/主题 **硬编码**，无用户新增通道、无单位、无语义 | 🔴 高优先级修复 |
| 曲线系统 | 单个固定 CurveWidget，**无多窗口/无曲线管理/无 LTTB 接入/无 1kHz 验证** | 🔴 高优先级修复 |
| 自动化 | AutomationEngine **为桩实现**（loadTestCase/executeStep 返回 false），无流程图 UI、无报告 | 🔴 高优先级修复 |
| 动态控件 | 无用户可创建的 Button/Slider/Input 绑定命令 | 🟡 中优先级 |
| UI 布局 | 单一 QTabWidget，无"左导航/中工作区/右属性/底日志"工业布局 | 🟡 中优先级 |
| 测试 | `test_main.cpp` 为占位；`test_phase3.cpp` 为真实链路基准但未接入 CI/CTest 全量门禁 | 🟡 中优先级 |
| 线程模型 | **全单线程**（UI 线程），设计文档的 10 线程模型未落地 | 🔴 高优先级修复 |

**总体判断**: v0.1 是**数据通路正确、模块骨架齐全**的工程 Demo。核心缺陷不在"有没有代码"，而在**架构未贯通**（DataBus/CurveEngine 未接入）、**能力未实现**（自动化是桩）、**可扩展性缺失**（通道/曲线硬编码）、**构建系统双轨且 CMake 断裂**。

---

## 1. 现状总览

### 1.1 已实现（v0.1，TASK_BOARD 全 🟢）

```
串口连接/断开/状态指示
VOFA+ 协议解析（JustFloat / FireWater）
多页面 QTabWidget（实时曲线 / 仪表盘 / 故障 / 参数）
实时曲线（多通道、2000点降采样显示、缩放平移、图例）
仪表盘（数值格、警告阈值变色）
参数（ParameterManager + 编辑面板 + JSON 导入导出）
故障列表（硬件/软件区分）
CSV 导出、曲线截图、工程文件 JSON 保存/加载
```

### 1.2 模块骨架（79 个源文件，11 个 src 子目录）

| 目录 | 内容 | 状态 |
|------|------|------|
| `app/` | Application | 桩（initialize 仅置 running=true） |
| `communication/` | ITransport + Serial/CAN/TCP/UDP/Loopback 五种传输；IProtocol + FrameCodec + MotorProtocol + VofaParser + CommandQueue | ✅ 较完整 |
| `core/` | EventBus、ModuleManager、SettingsManager、Message | ✅ 存在 |
| `curve/` | CurveEngine（环形缓存+LTTB，**未接入 UI**）、CurveRenderer | 🟡 引擎在、渲染未接 |
| `data/` | DataBuffer、DataManager、MotorData、DataRecorder | 🟡 未接入主链路 |
| `databus/` | DataBus、ChannelManager、ChannelRingBuffer、RingBuffer(无锁SPSC)、Topic | 🟡 双轨并行 |
| `device/` | DeviceSimulator（500Hz 模拟器，含故障/电机模型） | ✅ 可用于 Mock |
| `logging/` | Logger（6 级 + 10 类 + 控制台/文件） | ✅ 存在 |
| `parameter/` | ParameterManager、ParameterTypes、ParameterWidget | ✅ 存在 |
| `plugin/` | IPlugin + PluginLoader（C ABI 边界） | 接口在，无插件 |
| `scripting/` | IScriptEngine | 仅接口 |

---

## 2. 差距分析（含证据）

### G1 🔴 构建系统与设计严重不符，CMake 断裂

| 项 | 设计文档 | 实际 | 证据 |
|----|----------|------|------|
| 语言标准 | C++20 | **C++17** | `motor_antomation.pro:5` |
| Qt 版本 | Qt 6.6+ | **Qt 5.14.2 MinGW 32-bit** | `motor_antomation.pro:8-9`；`CMakePresets.json` |
| 构建系统 | CMake | **qmake 为主**（CMake 无顶层文件） | 仓库根与 `motor_antomation/` 均无 `CMakeLists.txt`，仅 `src/CMakeLists.txt`、`tests/CMakeLists.txt` |
| 目标平台 | Windows + Linux | 仅 Windows 32-bit | `.pro` 的 `win32-g++` 分支 |

**影响**: 设计文档第 2 章目录树（cmake/toolchains、tests/、libs/）与实现脱节。CMake 版本**无法 configure**（缺顶层入口），只能 qmake 构建。双轨构建 = 双倍维护成本；32-bit Qt5 对 1kHz×100 通道的内存/性能目标不友好。

### G2 🔴 数据管道未走 DataBus，架构"红线条"未落地

- **现状**: `mainwindow.cpp:332-365` 直接 `SerialTransport→VofaParser→ChannelManager→CurveWidget/Dashboard`，全部在 UI 线程。`DataBus`、`CurveEngine`、`DataManager` 三个核心组件**存在于代码但未接入应用主链路**（`mainwindow.cpp` 未 include databus/DataBus.h、curve/CurveEngine.h）。
- **证据**: `test_phase3.cpp` 走的是另一条路（`LoopbackTransport→DataManager→CurveEngine`），与 MainWindow 的 `ChannelManager→CurveWidget` 是**两套平行数据系统**。
- **违背设计**: 附录 A.5 红线条 2 "DataBus 为唯一数据通道"、红线条 5 "Pub/Sub 回调异步分发"。当前 ChannelManager 无锁、无订阅模型、无广播语义。
- **影响**: 曲线引擎（环形缓存/LTTB）白白存在却没用；未来加 Recorder/Automation 订阅者时无处挂载。

### G3 🔴 无动态通道系统（VOFA+ 核心要求）

- **现状**: `VofaParser.h:29 setChannelCount(int)` 需事先固定通道数；`ChannelManager::pushFrame` 按帧长度自动建通道，但通道名是通用名（无语义、无单位）；`Topic.h:29-41` 主题枚举硬编码 `Ia..Timestamp` 共 12 个。
- **缺口**: 用户新增通道（如 "Torque"）→ 自动出现对应曲线；通道的**名称/单位/缩放系数/数据类型**由协议或用户配置驱动——全部未实现。
- **证据**: `Topics` 常量表（`Topic.h:29-41`）、`ChannelManager::addChannel` 仅接收 name 字符串无单位/类型。

### G4 🔴 曲线系统为"单控件"，非"多窗口 + 曲线管理"

- **现状**: `ui/CurveWidget.h` 单 QWidget，`kMaxDrawPoints=2000`（`CurveWidget.h:92`），纯 QPainter 绘制；`CurveEngine.h:90` 有 LTTB `downsample()` 但 **CurveWidget 未使用**。
- **缺口**（对照 VOFA+ / 用户要求）:
  - 多窗口 Tab/Grid 模式（窗口1: Ia/Ib/Ic；窗口2: Speed/Position…）❌
  - Curve Manager：添加/删除/改色/改名/单位/Y轴范围/显示隐藏 ❌（CurveWidget 只有 addChannel/removeChannel/setChannelColor，无单位、无管理面板）
  - 100Hz/500Hz/1kHz 高刷与多线程安全 ❌（单线程 + 2000 点截断）
  - 游标测量、峰值/谷值标记、RMS 等内置分析 ❌

### G5 🔴 自动化测试为桩实现（P0 需求却空转）

- **证据**: `AutomationEngine.cpp` 全文 40 行——`loadTestCase()` 返回 false、`loadTestSuite()` 返回 false、`executeStep()` 返回 false、`registerCustomStep` 空实现；`TestRunner.cpp` 仅 26 行。无 JSON 用例解析、无步骤执行、无断言、无报告、无自动化 UI/流程图。
- **影响**: 生产测试的核心价值（无人值守、断言、报告）为零。TASK_BOARD 将自动化划入 v0.5，但 v0.5 并未开始。

### G6 🟡 无动态控件系统

- 用户需能创建 Button（绑定 Run Script HighSpeed_Test）、Slider（绑定 Set Motor Speed）、Input（绑定 Set Voltage）等**动态绑定控件**。
- **现状**: `parameter/ParameterManager.h:73` ParameterWidget 是参数查看/编辑面板，无命令绑定、无用户自定义控件。

### G7 🟡 UI 不是工业级布局

- **现状**: `mainwindow.cpp:193-204` 单一 `QTabWidget` 作为 central widget。
- **目标**: 左导航（Dashboard/Oscilloscope/Automation/Device/Protocol/Settings）+ 中间 Workspace + 右侧 Property Panel + 底部 Log Console（类 Qt Creator / 工业 IDE）。
- Dashboard 卡片化（电机状态/通信状态/电压/电流/转速/温度/故障）**部分存在**（DashboardWidget 数值格 + 阈值变色，TASK_BOARD DASHBOARD-03 🟢），但无"通信状态/电机状态"卡片、无卡片布局管理。

### G8 🟡 测试体系不满足"完整测试体系"

- **证据**: `tests/test_main.cpp` 仅 `testPlaceholder() { QVERIFY(true); }`；`tests/test_phase3.cpp` 是**真实的数据链路基准**（Loopback + 500Hz 模拟器，60s，统计丢包率/延迟百分位/曲线缓存），质量不错，但：
  - 无单元测试覆盖 VofaParser/ChannelRingBuffer/CurveEngine/ParameterManager 等核心类
  - 无 Mock MCU 驱动 UI 曲线验证（用户要求"模拟 MCU 数据 → 验证曲线显示"）
  - CTest 已注册但顶层 CMake 缺失 → **测试无法在 CI/命令行统一跑**
  - 无自动化断言在 UI 层验证

### G9 🔴 线程模型未落地（设计文档第 12 章 vs 现实）

- **设计**: UI / Communication / DataBus / Curve / Logger / Automation / Script / Recorder / Update 多线程，无锁队列、优雅关闭顺序（`design doc` §12）。
- **现实**: 全 UI 线程。`QSerialPort` 在 main 线程；`ChannelRingBuffer`（`ChannelManager.h:27-55`）是**无锁保护**的裸 QVector + int 索引（今天单线程所以没崩，但不具备可扩展的线程安全）；`CurveChannel`（`CurveEngine.h:52`）反而有 mutex——**两个缓冲实现，线程安全语义不一致**。
- **风险**: 接入 1kHz + Recorder + Automation 后 UI 必然卡顿；违背"通信线程不直接更新 UI"。

### G10 🟢 已达标/低风险项

- 协议预留 ✅：`ITransport` 抽象 + Serial/CAN/TCP/UDP 四类传输已具备，满足"支持未来 UART/CAN/TCP 不绑定具体协议"（传输已实现，仅协议适配层待接入）。
- 插件/脚本为接口层，符合"预留不实现"策略。
- 日志、参数、工程文件、CSV 导出基础功能可用。
- `DeviceSimulator`（500Hz、电机模型、故障注入）可作为 Mock MCU 复用。

---

## 3. 保留资产（升级不推倒重来）

| 资产 | 用途 | 升级去向 |
|------|------|----------|
| `communication/transport/*`（5 种 ITransport） | 传输抽象 | 保留，作为管道底座 |
| `communication/protocol/MotorProtocol`（CRC+状态机） | 二进制帧编解码 | 保留 |
| `curve/CurveEngine`（环形缓存+LTTB+线程安全 append） | 曲线数据引擎 | **接入**为主数据源 |
| `databus/RingBuffer`（无锁 SPSC）+ `DataBus` | 数据总线 | 统一接入应用链路 |
| `device/DeviceSimulator` | Mock MCU | 复用为测试夹具 |
| `tests/test_phase3.cpp` | 链路基准 | 移植为 CTest 门禁用例 |
| `logging/Logger`、`parameter/ParameterManager` | 基础服务 | 保留 |
| `docs/` 14 篇设计文档 | 架构知识 | 归档；另生成用户要求的 5 篇平台文档 |

---

## 4. 升级路线（Phase 2 输入）

### 决策点（需用户在开工前确认）

| # | 决策 | 选项 | 建议 |
|---|------|------|------|
| D1 | **工具链** | A) 继续 Qt5.14/C++17（当前可用） B) 迁移 Qt6/C++20/CMake | **建议 B 作为 P0 主轨道**（与设计文档一致、目标平台 Linux 需要 CMake）；若本机暂不可装 Qt6，则 P0 先统一 CMake 结构、保持 Qt5 兼容代码，Qt6 作为独立迁移阶段 |
| D2 | **构建统一** | A) 以 qmake 为主 B) 以 CMake 为主 | **建议 B**：补齐顶层 CMakeLists.txt + Presets，qmake 废弃 |
| D3 | **管道统一** | A) ChannelManager 继续为 UI 数据源 B) DataBus+CurveEngine 为唯一数据通道，ChannelManager 退化为兼容适配层 | **建议 B**（设计红线条 #2） |

### P0 — 架构贯通（构建 + 管道 + 线程）
1. 补齐顶层 CMakeLists.txt、统一构建（D1/D2 定案后执行）；建立可复现 build + ctest 门禁
2. 统一数据管道：`Transport → VofaParser/MotorProtocol → DataBus(广播) → CurveEngine/DataManager/UI`；接入现有 CurveEngine
3. 确立线程模型 v1：Communication 线程 → 无锁队列 → UI 线程（QueuedConnection）；CurveEngine 可跨线程 append
4. 补充 `tests/` 单测框架（QtTest）挂入 CTest

### P1 — Curve Engine 升级（VOFA 级）
1. **Dynamic Channel System**：ChannelRegistry，通道=名称/单位/类型/缩放/颜色，协议驱动自动注册，用户可增删；Topics 去硬编码
2. **多窗口**：Tab/Grid 布局容器 + 每窗口独立 CurveWidget
3. **Curve Manager 面板**：添加/删除/改色/改名/单位/Y轴范围/显示隐藏
4. **性能**：LTTB 接入渲染、1kHz 验证、环形缓冲容量配置化、多线程安全

### P2 — UI Framework
1. 工业布局：左导航 + 中央 Workspace（多页面复用）+ 右属性面板 + 底日志控制台
2. Dashboard 卡片化升级（电机状态/通信状态卡片）
3. 动态控件系统：Button/Slider/Input 创建器 + 命令绑定（绑定 Set Speed / Run Script 等）

### P3 — Automation
1. 流程引擎落地：TestRunner 完整实现（步骤执行/断言/循环/分支/超时重试）
2. 流程图 UI：节点式编辑器（Start→Set Speed→Wait→Check→Record→Stop）
3. 报告生成（HTML/CSV）+ 异常报警策略（调试/测试双模式）

### P4 — Testing（贯穿，随各优先级增量）
1. Unit Test：VofaParser、RingBuffer、CurveEngine、ParameterManager、ChannelManager
2. Integration Test：Mock MCU（DeviceSimulator）→ 全管道 → 曲线显示断言
3. CI 门禁：build + ctest + 性能基准（test_phase3 移植）

---

## 5. 验收矩阵（成功标准）

| # | 能力 | 验收标准 |
|---|------|----------|
| A1 | 可复现构建 | `cmake --preset x && cmake --build && ctest` 全绿，qmake 废弃 |
| A2 | 数据管道统一 | DataBus 为唯一数据通道；UI 通过订阅获取数据；无模块直连 Transport |
| A3 | 动态通道 | 协议新增通道（如 Torque）→ UI 自动出现曲线，无需改代码 |
| A4 | 多曲线多窗口 | 至少 3 个窗口（Tab/Grid），每窗口多通道，独立 Y 轴 |
| A5 | 曲线管理 | 添加/删除/改色/改名/单位/Y轴范围/显隐全部可用 |
| A6 | 高刷安全 | 1kHz 数据下 UI 不掉帧、无崩溃；缓冲容量可配置 |
| A7 | 自动化 | 表格/流程图定义用例 → 执行 → 断言 → 报告闭环 |
| A8 | 动态控件 | 创建 Button/Slider/Input 并绑定命令/脚本可用 |
| A9 | 工业 UI | 左导航+工作区+属性+日志布局；Dashboard 卡片化 |
| A10 | 测试体系 | ctest 覆盖 Unit+Integration；Mock MCU 曲线显示断言通过 |
| A11 | 线程安全 | 通信线程不直接更新 UI；CurveEngine 跨线程 append 正确 |

---

> **报告结束**。下一步（Phase 2）：确认 D1/D2/D3 决策 → 制定正式 Roadmap 文档 → 开始 P0。
