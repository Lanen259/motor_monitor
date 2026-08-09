# 自动化域夜间防卡死自测 — 最终报告

> 域：domain/automation ｜ 分支：domain/automation ｜ 执行日期：2026-08-09 ｜ 本轮通宵全自动完成

## 0. 执行日志摘要

| 轮次 | 阶段 | 产出 | 结果 |
|---|---|---|---|
| 1 | Phase 0 + Phase 1 | 基线报告 + 审计清单 | ✅ 工具链定位、gate 基线、16 项缺陷（A-01~A-17） |
| 2 | Phase 2 + Phase 3 | 复现测试 + G1-G7 修复 + A-16 真根因 | ✅ 3 个 UI 测试全绿、gdb 实证 A-17 死循环 |
| 3 | Phase 4 压力测试 + 完整门禁 | stress ×3 种子 + 18/18 ctest | ✅ 看门狗零触发、gate PASSED |
| 4 | 代码审查 + H1 修复 | H1 元类型 + 构造顺序 + 按钮刷新 + 守卫复位 | ✅ H1 回归测试绿、最终 gate PASSED |
| 5 | CI 失败根因修复 | `.gitignore` 的 `ui_*.h` 模式误忽略 `ui_watchdog.h` → 重命名 `watchdog.h` | ✅ CI 绿（c54701b） |

---

## 1. 任务回顾

自动化域（AutomationWidget / FlowCanvas / NodeLibraryPanel / NodeParamPanel / VariableEditorPanel）在任意用户交互下零卡死、零崩溃。目标：SYM-1/2/3 三个用户症状根因闭环，全测试绿，合入 master。

## 2. 关键成果

### 2.1 SYM 卡死根因实证与闭环（最高价值）

- **A-17（gdb 实证）**：`NodeParamPanel::clearForm` 的 `while (rowCount() > 0) { takeAt(0); }` 在表单含整行控件（`addRow(QWidget*)`）与空标签行（`addRow(QString(), field)`）时，`QFormLayout::count()` 与 `rowCount()` 失同步，`takeAt(0)` 先返回 null 而 `rowCount()` 仍 > 0 → **死循环**（零 CPU 阻塞、`Invalid index 0` 警告洪泛）。
- 触发条件：**任何第二次 buildForm**（切换节点 / 删除参数行 / 添加参数）→ UI 线程卡死。这就是 SYM-1/2/3「卡死」的当前活跃根因（历史 BUG-004 修复引入的 clearForm 写法不收敛）。
- 修复：循环条件改 `while (count() > 0) { takeAt(0) }`；widget 用 `disconnect()+hide()+deleteLater()`（同时解决 A-01/A-02 删除信号发送者 UAF）。

### 2.2 统一修复（同根因同方案，G1-G7）

| 组 | 缺陷 | 方案 |
|---|---|---|
| G1 | A-17 死循环 / A-01 / A-02 UAF | clearForm count()+deleteLater |
| G2 | A-03 / A-06 / A-10 | deleteSelectedItems 入射边去重+模型同步+拖拽状态取消+nodeDeselected |
| G3 | A-07 / A-13 | addNodeFromPalette 先入模型再选择 |
| G4 | A-11 | paramsChanged 只刷新选中节点 |
| G5 | A-04 / A-05 | runFlowGraph 重入守卫 + 析构清理 |
| G6 | A-14 | 绑定 VariableScope 到变量表 |
| G7 | A-15 | 参数表键编辑行索引原位更新 + 非法键回退 |

### 2.3 代码审查 H1 阻塞项（存量缺陷升级）

- `FlowRunResult` 未注册 Qt 元类型 → `runnerFinished` 跨线程投递失败 → 流程运行完成路径整体失效（运行按钮一次后永久锁定、worker 泄漏）。
- 修复：`Q_DECLARE_METATYPE` + `qRegisterMetaType`；构造顺序修复（先切流程图视图再算按钮状态）；`graphChanged`→按钮刷新；守卫复位接入 `QThread::finished`。
- 附带修复存量功能缺陷：双击节点库添加节点后运行按钮不启用。

### 2.4 A-16 真根因

`test_automation_engine.cpp:31` 的 `const char* dataDir = qgetenv(...)` 悬垂指针（qgetenv 返回临时 QByteArray）→ 读取损坏路径 → TestAutomationEngine 失败/偶发挂起。修复为保存 QByteArray 生命周期。另对 `AutomationEngine::run()` 值拷贝与 `TestRunner` 析构加防御。

## 3. 测试体系（Phase 2 + Phase 4）

| 测试 | 覆盖 | 结果 |
|---|---|---|
| TestNodeParamPanelInteraction（7 用例） | A-01/A-02 UAF、A-15、A-17 闭环（setNode/clearNode×50）、通用表 | 绿 |
| TestFlowCanvasInteraction（11 用例） | A-03、A-06、A-07、SYM-1/2/3、H1 运行完成、拖拽看门狗、添加计数 | 绿 |
| TestVariableEditorPanelInteraction（5 用例） | A-14 作用域、编辑功能、防卡死 | 绿 |
| StressAutomationUi ×3 种子 | ≥500 次随机交互 + 看门狗 300ms + 状态自检 | 绿（看门狗零触发） |
| TestAutomationEngine 等既有 12 项 | 无回归 | 绿 |

- 看门狗：`tests/common/ui_watchdog.h`（共享规格 §2.2，心跳 50ms，阈值 300ms，双信号：投递延迟 + 执行间隔）。

## 4. 门禁与验证

| 检查 | 结果 |
|---|---|
| gate（qmake 全量编译 + CMake 测试构建 + ctest） | **[GATE] PASSED**（18/18 全绿） |
| ctest --output-on-failure | 18/18 Passed |
| 压力测试 3 种子 | 全绿，看门狗零违规 |
| D4 静态检查 | clang-tidy 头文件解析受限（Windows 环境），cppcheck 未安装；以编译器告警（零新增）+ 独立代码审查替代 |
| 代码审查 | 独立 agent 完成，H1 阻塞项已修复，L1 已处理，其余记录 |
| CI（GitHub Actions） | 见 §7 |

## 5. 需集成代理处理（红线文件，域内不改）

1. **A-05/M1 关闭时运行残留窗口**：`AutomationWidget` 析构 `wait(2000)` 在 worker 阻塞于跨线程回调时会超时（worker 仍持有 `m_varScope`/引擎引用）。建议集成代理在主 checkout 调整 MainWindow 引擎生命周期（运行中关闭 → 先 stop+wait 再删引擎）。
2. **L2 存量竞态**：`AutomationEngine::run()` 中 `d->currentStepIndex` 写、回调成员读无锁（域内已值拷贝缓解主路径），后续可统一加锁。
3. **`tests/` 归属**：本夜在 automation worktree 下新增/修改 `tests/unit/ui/`、`tests/stress/`、`tests/common/`、`tests/CMakeLists.txt`（任务书 §5 授权）。如集成代理认为 tests/ 应归 master，请合并后统一裁决。

## 6. 提交记录

| 提交 | 内容 |
|---|---|
| `ca88a0d` | Phase 0 基线报告 |
| `1a3636b` | Phase 1 只读审计（16 项缺陷清单） |
| `c18098e` | Phase 2 复现测试 + Phase 3 统一修复（G1-G7）+ Phase 4 压力测试 + 审查 H1 修复 + 报告 |

## 7. §11 验收矩阵

| # | 验收项 | 证据 | 状态 |
|---|---|---|---|
| A1 | Phase 0 基线报告 | reports/nightly_baseline_automation.md | ✅ |
| A2 | 缺陷审计清单（含 SYM-1/2/3 根因） | reports/audit_automation.md | ✅ |
| A3 | 回归用例齐备且修复前红/修复后绿 | ctest 输出 + git 提交对照（A-17 死循环由 gdb 实证红→绿；A-06/A-14/A-15 红→绿） | ✅ |
| A4 | 统一修复完成，同根因同方案 | git log（c18098e，G1-G7 + H1） | ✅ |
| A5 | 压力测试 3 种子全绿 | ctest StressAutomationUi_seed1/2/3 | ✅ |
| A6 | gate 全绿（编译+测试） | [GATE] PASSED | ✅ |
| A7 | 最终报告 + push 完成 | reports/nightly_report_automation.md + git ls-remote（domain/automation = c54701b） | ✅ |
| D6 | GitHub CI 绿 | Actions 页面（c54701b1 = success） | ✅ |

## 8. 遗留事项

### 8.1 合并 master 被阻塞（需集成代理裁决）

已按任务书第 7 步在主 checkout 尝试 `git merge origin/domain/automation --no-ff`，**出现冲突已 abort + 删锁**（未自行解决 master-owned 文件冲突，遵守红线）。

**冲突文件与性质**（均为 `tests/` 测试基建，非产品代码）：

1. **`motor_antomation/tests/CMakeLists.txt`（content）**：master 已包含**波形域**夜间任务新增的测试目标（`TestCurveWidgetInteraction`、`TestHistoryReplayInteraction`、`add_ui_interaction_test` 宏等）；与自动化域新增的 UI/压力测试目标冲突。两者**加性可合**（波形测试 + 自动化测试并存），需集成代理合并去重。
2. **`motor_antomation/tests/common/watchdog.h`（add/add）**：master 与 automation 各自创建了共享看门狗头文件（共享规格 §2.2）。需集成代理统一实现（实现逻辑应一致，阈值 300ms）。
3. **`motor_antomation/tests/unit/automation/test_automation_engine.cpp`（content）**：**master 已独立修复同一缺陷**（提交 `90e328d`：qgetenv 悬垂指针），与本分支修复一致 → 冲突可简单消解（保留任一即可）。

**建议给集成代理的处理方案**：按"波形测试 + 自动化测试并存"合并 CMakeLists（删除重复的 `add_ui_interaction_test` 宏或统一）；看门狗头文件取一版本（建议统一为共享规格参考实现）；test_automation_engine.cpp 保留 master 的 `90e328d` 版本（等效）。

**域分支状态**：`domain/automation` = e9ea894，已 push，**CI 绿**，18/18 ctest 全绿，gate PASSED，§11 验收矩阵全绿 —— 具备合并条件，仅因与波形域并发开发的测试基建冲突而待集成代理执行合并。

### 8.2 其他遗留

- **A-05/M1 关闭时运行残留窗口**：`AutomationWidget` 析构 `wait(2000)` 在 worker 阻塞于跨线程回调时会超时。建议集成代理在主 checkout 调整 MainWindow 引擎生命周期（运行中关闭 → 先 stop+wait 再删引擎）。
- **L2 存量竞态**：`AutomationEngine::run()` 中 `d->currentStepIndex` 写、回调成员读无锁（域内已值拷贝缓解主路径），后续可统一加锁。
- **USER_MANUAL.md 同步**：合入 master 后由集成代理统一更新（涉及运行按钮行为、变量表功能、流程运行完成态）。
- **Bug 登记**：A-17 死循环、H1 元类型缺失等已在报告中闭环，建议集成代理在 master 的 `motor_antomation_Bug_Report.md` 补充登记。
