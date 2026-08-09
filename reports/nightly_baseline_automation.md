# 自动化域夜间防卡死自测 — Phase 0 基线报告

> 日期：2026-08-09 ｜ 域：domain/automation ｜ 工具链：Qt 5.14.2 mingw73_32 + MinGW 7.3.0 (i686-posix-dwarf) ｜ 平台：Windows 11 (Git Bash + offscreen)

## 1. 环境准备

| 项 | 结果 | 备注 |
|---|---|---|
| git status | 干净 | 无本任务外未提交改动，无需 stash |
| git merge master | 已完成 | master 已是 domain/automation 祖先（`Already up to date`），分支落后 0 提交 |
| 工具链定位 | Qt=`D:/Program_flies/qt_creat/APP/5.14.2/mingw73_32`，MinGW=`D:/Program_flies/qt_creat/APP/Tools/mingw730_32/bin` | 非 gate.bat 默认路径，用 `GATE_QT_DIR`/`GATE_MINGW_BIN` 覆盖 |

> 注意：`scripts/gate.bat` 的默认工具链路径（`D:/Program_Files/QT5.14/...`、`C:/MinGW/bin`）在本机不存在。实际工具链位于 `D:/Program_flies/qt_creat/...`。已在 `build/nightly/gate.sh`（git-ignored）实现 gate.bat 三步的 bash 等价版，供本轮全自动循环复用。

## 2. 基线构建与测试结果

执行 `build/nightly/gate.sh`（= gate.bat 三步：qmake 全量编译 → CMake 测试构建 → ctest）：

| 步骤 | 结果 |
|---|---|
| 1/3 qmake 全量编译（motor_antomation.pro） | PASSED |
| 2/3 CMake 测试构建（build/ci-tests） | PASSED |
| 3/3 ctest（12 个测试） | **11/12 PASSED，1 FAILED** |

### 2.1 ctest 明细

| 测试 | 结果 | 耗时 |
|---|---|---|
| MotorStudioTests | Passed | — |
| MotorStudioPhase3Test | Passed | — |
| TestTopicRegistry | Passed | — |
| TestDataBus | Passed | — |
| TestCurveEngine | Passed | — |
| TestVofaParser | Passed | 1.90s |
| **TestAutomationEngine** | **Failed（exit 4 / 偶发无限挂起）** | 3.41s（一次）/ 超时（另一次） |
| TestVariableScope | Passed | 2.58s |
| TestFlowGraph | Passed | 1.98s |
| TestExpressionEngine | Passed | 1.54s |
| TestPipelineIntegration | Passed | 5.88s |
| TestMockMcuCurve | Passed | 8.35s |

## 3. 已知失败项（预存，非本轮引入）

### FAIL-A：TestAutomationEngine 不稳定失败/挂起

- **现象**：同一二进制多次运行表现不同 —— 有时 ~3.4s 内退出且 exit code=4（有测试函数失败），有时无限挂起（>120s 不退出，需 taskkill）。ctest 下一次失败、一次挂起。
- **涉及代码**：`tests/unit/automation/test_automation_engine.cpp`（测试方）+ `src/automation/TestRunner.cpp`（线程管理）+ `src/automation/ReportGenerator.cpp`（测试完成后同步文件 IO）。
- **初步判断**：`TestRunner` 把 `AutomationEngine` 迁移到 worker 线程后，`setCurrentTestCase()` 从主线程直接调用（数据竞争）；`runAsync` 结束后 `onEngineTestCompleted` 在主线程同步生成 HTML/CSV 报告（文件 IO + QThread 清理 `wait(3000)`），存在竞态/死锁窗口。属 P5/P1 类问题，**在本域内**，计划在本轮 Phase 3 统一修复（见后续审计报告）。
- **影响**：阻塞 D2 门禁（`ctest` 全绿）。已作为「阻塞项」登记，后续处理。

### 环境提示：QTest 输出不可见

所有测试在 ctest/直接运行时 stdout 均为空（GUI 子系统 + MSYS 控制台句柄丢失），只能靠退出码判定 pass/fail。对本轮新 UI 测试的调试意味着：**通过退出码 + 自定义日志文件（如 QFile 写 reports/ 下的调试文件）来取证**，不依赖 stdout。

## 4. 基线结论

- 编译环境可用，全量编译通过。
- 12 个测试中 11 个绿；1 个预存失败（TestAutomationEngine，域内，归入本轮修复范围）。
- UI 测试统一 `QT_QPA_PLATFORM=offscreen`，无硬件依赖。
- Phase 0 验收达成：基线报告产出；gate 能跑出明确结果并记录了失败项。
