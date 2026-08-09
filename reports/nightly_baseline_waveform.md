# 波形域夜间防卡死自测 — Phase 0 基线报告

> 生成时间：2026-08-09（夜间自测开始）
> 分支：`domain/waveform` ｜ worktree：`E:\My_project\QT\Motor_Antomation-wt\waveform`
> 任务书：《波形.md》v1.0（本域） + 《夜间防卡死自测_共享执行规格.md》v1.0

---

## 1. 环境与工具链

| 项 | 值 |
|---|---|
| 编译器 | MinGW-W64 **7.3.0 (i686-posix-dwarf, 32-bit)** |
| Qt | **5.14.2 mingw73_32** |
| CMake | `D:\Program_flies\Cmake\bin\cmake.exe` |
| qmake | `D:\Program_flies\qt_creat\APP\5.14.2\mingw73_32\bin\qmake.exe` |
| g++/mingw32-make | `D:\Program_flies\qt_creat\APP\Tools\mingw730_32\bin\` |
| UI 测试平台 | `QT_QPA_PLATFORM=offscreen` |

> ⚠️ **关键环境发现**：`scripts\gate.bat` 默认工具链路径（`D:/Program_Files/QT5.14/...`、`C:/MinGW/bin`）在本机**不存在**。
> 实际工具链位于 `D:\Program_flies\qt_creat\APP\...`。必须通过环境变量覆盖：
> ```bat
> set GATE_QT_DIR=D:/Program_flies/qt_creat/APP/5.14.2/mingw73_32
> set GATE_MINGW_BIN=D:/Program_flies/qt_creat/APP/Tools/mingw730_32/bin
> ```
> 后续所有 gate / static-analysis / ctest 调用均带这两个覆盖。

## 2. Git 状态

- 分支：`domain/waveform`，工作区**干净**（无未提交改动）。
- `git merge master`：HEAD 已是最新合并提交 `3f9f000 Merge branch 'master' into domain/waveform`，与 master（`33b0b32`）已同步，**无需再合并**。

## 3. 基线构建结果

`scripts\gate.bat`（带工具链覆盖）三步全绿：

1. **qmake 全量构建**：通过（0 错误）。
2. **CMake 测试构建**：通过，全部库 + 12 个测试可执行文件编译链接成功。
3. **ctest**：**12/12 全部通过**。

### 3.1 ctest 全量结果（基线）

| # | 测试 | 结果 | 耗时 |
|---|---|---|---|
| 1 | MotorStudioTests | PASS | 1.39s |
| 2 | MotorStudioPhase3Test | PASS | 63.78s |
| 3 | TestTopicRegistry | PASS | 2.45s |
| 4 | TestDataBus | PASS | 2.61s |
| 5 | TestCurveEngine | PASS | 2.43s |
| 6 | TestVofaParser | PASS | 1.86s |
| 7 | TestAutomationEngine | PASS | 6.36s |
| 8 | TestVariableScope | PASS | 2.75s |
| 9 | TestFlowGraph | PASS | 1.97s |
| 10 | TestExpressionEngine | PASS | 1.62s |
| 11 | TestPipelineIntegration | PASS | 6.41s |
| 12 | TestMockMcuCurve | PASS | 10.41s |

```
100% tests passed, 0 tests failed out of 12
Total Test time (real) = 104.09 sec
```

### 3.2 已注册测试清单（ctest -N，基线 12 个）

MotorStudioTests、MotorStudioPhase3Test、TestTopicRegistry、TestDataBus、TestCurveEngine、TestVofaParser、TestAutomationEngine、TestVariableScope、TestFlowGraph、TestExpressionEngine、TestPipelineIntegration、TestMockMcuCurve。

## 4. 基线已知问题

- 当前**尚无任何波形域 UI 交互 / 卡死 / 压力测试**（现有 12 个测试均为单元 / 集成级，不覆盖 UI 交互与高频渲染路径）。这正是本夜 Phase 2/4 要补齐的。
- `MotorStudioPhase3Test` 耗时 63.78s（接近默认 TIMEOUT 边界），本夜新增用例将设置显式 TIMEOUT 防挂起。

## 5. 结论

Phase 0 验收通过：基线报告产出、gate.bat 结果明确（编译全绿 + 12/12 测试通过）。下一步进入 Phase 1 只读审计。
