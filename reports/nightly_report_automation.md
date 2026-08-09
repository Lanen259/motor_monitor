# 自动化域夜间防卡死自测 — 循环报告

> 域：domain/automation ｜ 启动：2026-08-09 ｜ 每轮追加

---

## 第 1 轮（Phase 0 + Phase 1）：环境基线 + 只读审计

**时间**：2026-08-09 08:30–09:00

- **Phase 0**：merge master（已同步）、工具链定位（qt_creat 路径）、gate 基线 11/12 绿、TestAutomationEngine 预存失败。基线报告 `reports/nightly_baseline_automation.md`。
- **Phase 1**：4 独立审计 lens + synthesis 交叉验证，产出 16 项缺陷清单 `reports/audit_automation.md`（A-01~A-17）。含 2 个活跃 P1 UAF、线程竞态、P4/P6 风暴/不同步。
- **审计排除**：deleteSelectedItems 双重释放（悬垂非空指针不触发）、paramsChanged→graphChanged P3 环（无接收者）。
- **提交**：`ca88a0d`（Phase 0 基线）、Phase 1 审计。

## 第 2 轮（Phase 2 复现 + Phase 3 G1-G7 修复 + A-16 真根因）：UI 测试全绿

**时间**：2026-08-09 09:00–10:20

### 新增回归测试（修复前红）
- `tests/unit/ui/test_node_param_panel.cpp`：A-01 删参行 UAF、A-02 加参 UAF、BUG-001/004 闭环（setNode/clearNode×50）、A-15 键编辑重复、通用表编辑。
- `tests/unit/ui/test_flow_canvas.cpp`：A-06 删除模型同步、A-03 拖拽期删除、A-07 添加面板、SYM-1/2/3、A-08 拖拽看门狗、添加计数。
- `tests/unit/ui/test_variable_editor_panel.cpp`：A-14 变量表作用域、编辑功能、防卡死。
- 全部 `add_test()` 注册 + TIMEOUT 60；看门狗 `tests/common/ui_watchdog.h`（共享规格 §2.2，阈值 300ms）。

### gdb 实证根因（SYM 卡死真根因 A-17）
`NodeParamPanel::clearForm` 的 `while (rowCount() > 0) { takeAt(0); }` 在表单含整行控件/空标签行时，`QFormLayout::count()` 与 `rowCount()` 失同步，`takeAt(0)` 先返回 null 而 `rowCount()` 仍 > 0 → **死循环**（零 CPU 阻塞）。gdb 栈确认挂起在 clearForm:271。**任何第二次 buildForm（切换节点/删参行/加参）即 UI 线程卡死 —— 这是 SYM-1/2/3 的当前活跃根因。**

### G1-G7 统一修复（同根因同方案）
| 组 | 方案 | 覆盖 |
|---|---|---|
| G1 | clearForm 改 `while (count()>0){takeAt(0)}` + widget `disconnect()+hide()+deleteLater()` | A-17 死循环、A-01/A-02 UAF |
| G2 | deleteSelectedItems：入射边随节点删（去重）、同步数据模型、拖拽源被删→cancelEdgeDrag、删节点→emit nodeDeselected | A-03、A-06、A-10 |
| G3 | addNodeFromPalette 先入模型再 setSelected（单次 nodeSelected） | A-07、A-13 |
| G4 | paramsChanged 只刷新选中节点（refreshNodeItem） | A-11 |
| G5 | runFlowGraph 重入守卫 + 析构 stop/quit/wait | A-04、A-05 |
| G6 | AutomationWidget 创建并绑定 VariableScope 到变量表 | A-14 |
| G7 | paramTable 键列编辑按行索引原位更新 | A-15 |

### A-16 真根因（TestAutomationEngine 失败）
`tests/unit/automation/test_automation_engine.cpp:31` 的 `const char* dataDir = qgetenv(...)` —— qgetenv 返回临时 QByteArray，指针悬垂 → 读取损坏路径 → loadTestCase 失败（exit 4）。修复为保存 QByteArray 生命周期。另对 `AutomationEngine::run()`（解锁后引用改值拷贝）与 `TestRunner::~TestRunner`（运行中不阻塞迁移）做防御加固。

### 结果
- TestNodeParamPanelInteraction 7/7 绿、TestFlowCanvasInteraction 10/10 绿、TestVariableEditorPanelInteraction 5/5 绿。
- TestAutomationEngine 带 TEST_DATA_DIR 连跑 3/3 绿。

## 第 3 轮（Phase 4 压力测试 + 完整门禁）：全绿

**时间**：2026-08-09 10:20–11:00

- **压力测试**：`tests/stress/stress_automation_ui.cpp`（≥500 次随机交互 × 3 种子 + 看门狗 300ms + 状态自检 + 防空转断言 `m_realOps>=125`）。ctest 3 种子全绿（seed1 5.9s / seed2 5.4s / seed3 5.3s），看门狗零触发。
- **完整 gate**：qmake 全量编译 + CMake 测试构建 + ctest **18/18 全绿** → `[GATE] PASSED`。
- **A-16 真根因确认**：TestAutomationEngine 失败的根因是测试自身 `const char* dataDir = qgetenv(...)` 悬垂指针（qgetenv 返回临时 QByteArray）→ 读取损坏路径。修复后带 TEST_DATA_DIR 连跑 3/3 绿。
- **A-08/A-12（P4 事件风暴）验证**：压力测试含快速拖拽（8 mouseMove/次）与运行/停止操作，看门狗零违规 → 在 ≥10 节点规模下无 >300ms 卡顿，无需进一步节流（低风险项，由压力测试兜底）。
- **D4 静态检查**：clang-tidy 在本环境无法解析 Qt/工程头文件（Windows 路径问题，报 "QWidget file not found" 等环境性错误，非代码缺陷）；cppcheck 未安装。以编译器告警复核（仅 Qt 头文件弃用告警，改动文件零告警）+ 代码审查替代。

## 第 4 轮（独立代码审查 + H1 阻塞项修复）：全绿

**时间**：2026-08-09 11:00–12:00

### 代码审查（独立 agent）
G1-G7 七项修复经独立审查确认实现正确（clearForm 死循环消除、addNodeFromPalette 时序、deleteSelectedItems 模型同步、值拷贝等）。发现 1 个**阻塞项 H1** 与多个中/低项。

### H1（阻塞，存量缺陷升级）：FlowRunResult 未注册 Qt 元类型 → 流程运行完成路径整体失效
- `FlowRunner::runnerFinished(const FlowRunResult&)` 从 worker 线程经 `Qt::QueuedConnection` 投递到 UI，但 `FlowRunResult` 无 `Q_DECLARE_METATYPE`/`qRegisterMetaType` → Qt 丢弃该调用 → `onFlowRunnerFinished` 永不执行。
- **后果**：A-04 重入守卫永不复位（运行一次后按钮永久禁用）、状态卡"运行中"、worker 线程空转泄漏。这是存量缺陷，我的 A-04 把复位点绑在失效路径上使其升级。
- **修复**（3 处 + 1 加固）：
  1. `FlowRunner.h` 加 `Q_DECLARE_METATYPE(MotorStudio::FlowRunResult)`；`runFlowGraph` 里 `qRegisterMetaType<FlowRunResult>()`。
  2. **存量 bug**：构造函数先 `updateButtonStates`（此时视图还在表格模式 index 0）再切流程图 → 运行按钮从未启用。改为先 `setCurrentIndex(1)` 再算按钮状态。
  3. **存量功能缺陷**：双击节点库添加节点（`addNodeFromPalette`）不刷新按钮状态 → 运行按钮在添加节点后仍禁用。连接 `graphChanged` → 按钮状态刷新。
  4. **M2 加固**：重入守卫复位接入 `QThread::finished` 清理回调，与 runnerFinished 解耦。
- **回归测试**：`testRunFlow_completesAndUnlocks` —— 断言运行完成后"流程图汇总"标签出现 + 运行按钮恢复可用（H1 证明）。

### 其他审查项处理
- **L1**（A-15 键改空/_ 前缀致行映射脱钩）：已修 —— 非法键回退 UI 显示到原键（blockSignals 防重入），不写模型。
- **M1**（析构 wait 超时 → m_varScope UAF 残留窗口）：记为"需集成代理处理"（MainWindow 引擎生命周期）。
- **L7**（m_varScope 变量跨运行持久化）：设计选择，变量表作为实时编辑面，保留；已在报告中说明。
- **L2/L3/L5/L6**：存量/理论性，记录待后续。
- **D4**：clang-tidy 头文件解析受限（Windows 环境），cppcheck 缺失，以编译器告警 + 代码审查替代。

### 结果
- H1 回归测试绿；完整 ctest（含 H1 新用例）等待最终确认。

## 剩余清单
- [x] Phase 4 压力测试 3 种子全绿
- [x] 完整 gate + 18/18 ctest 全绿（修复前）
- [x] 代码审查 → H1 阻塞项修复 + L1 处理
- [ ] 最终完整 gate 确认（含 H1 新用例）
- [ ] 提交 + push + CI
- [ ] 最终报告 + 合入 master

## 阻塞项
- **cppcheck 不可用**（本机未安装）：D4 静态检查仅能以 clang-tidy 部分执行（且头文件解析受限），以编译器告警 + 代码审查替代。
- **A-05/M1 关闭时运行残留窗口**：worker 线程阻塞于跨线程回调时析构 `wait(2000)` 会超时，属残留窗口，建议集成代理在主 checkout 调整 MainWindow 引擎生命周期。
- **QTest 输出在 MSYS 下不可见**：靠退出码 + `-o` 文件取证；已用 `-o` 输出定位所有失败。
