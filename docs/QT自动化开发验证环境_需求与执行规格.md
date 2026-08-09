# QT 自动化开发验证环境 — 需求规格与执行计划

> 文档版本：v1.0 ｜ 编制日期：2026-08-08 ｜ 编制角色：专业工程师（需求方只提出模糊需求，由工程师补齐全部细节）
> 执行方式：本文档交给独立执行线程，**全自动执行直到全部验收通过**。除「C 盘文件操作」需人工审核外，其余操作（仓库内文件、GitHub 环境、构建验证）执行代理可自行完成。
> 目标项目：`E:\My_project\QT\Motor_Antomation`（GitHub: `github.com/Lanen259/motor_monitor`，SSH 远程已配置）
> 适用范围：本仓库 + **用户以后所有 QT 项目**（见 §5 完成定义与附录 B 新项目接入标准）

---

## 1. 文档目的与使用方式

### 1.1 背景一句话
用户的项目**每轮都让 agent 审核代码，但实际仍反复出 Bug**。根因：编译检查存在（qmake），但**行为验证（真正跑单元/集成测试）与静态检查从未机器化执行过**——审核是"观点"，门禁必须是"事实"。

### 1.2 本文档解决什么
把用户"QT 项目改完后要能自动验证"的模糊需求，落成一份**另一个线程可以照此全自动执行**的规格：
1. 所有 QT 项目修改后，必须通过 **qmake 编译 + ctest 全部测试 + 静态检查** 才算完成。
2. 每次 push/合并，由 **GitHub Actions** 自动重复上述验证，失败即红。
3. agent 可**在本地（用户电脑的 Qt）模拟/调试**，也可**通过 GitHub CI 验证**。
4. **clang-tidy / cppcheck** 接入代码审查环节（Superpowers 的 code review 步骤），静态检查也进合并门禁。
5. 全部自动化，唯一的**人工审核点 = C 盘文件操作**（安装软件、写系统目录等）。

### 1.3 角色定义
| 角色 | 职责 |
|---|---|
| 需求方（用户） | 只提模糊需求，不参与技术决策；只在 C 盘操作时被请求审核 |
| 执行代理（另一线程） | 按本文档 §6 逐步执行，直到 §7 验收矩阵全绿 |
| 集成代理（项目内） | 按 `CLAUDE.md` 治理公约，负责 master 上的壳层/中枢/接线与合并裁决 |

### 1.4 成功定义
§7 验收矩阵中所有条目全部通过 = 本任务成功。执行代理每完成一个 Phase 必须留下验证证据（命令输出、CI 截图链接、测试统计）。

---

## 2. 需求背景与现状盘点

### 2.1 项目技术栈（不可变更的硬约束）
| 项 | 值 | 来源 |
|---|---|---|
| 框架 | Qt 5.14.2 | 决策 D1/D2（conductor ledger） |
| 语言/标准 | C++17 | `motor_antomation.pro` |
| 主构建 | **qmake**（CMake 仅作测试门禁，不迁移） | 决策 D2 |
| 编译器 | MinGW 7.3.0 **32 位**（i686） | `CMakePresets.json` |
| 工具链路径 | Qt: `D:/Program_Files/QT5.14/APP/5.14.2/mingw73_32`；MinGW: `C:/MinGW` | `CMakePresets.json` |
| 目标平台 | Windows | 设计文档 |

### 2.2 已有多 agent 基础设施（保留不动）
- **worktree 拓扑**：`master`（主 checkout `Motor_Antomation`）+ `domain/{waveform,automation,comms}`（`Motor_Antomation-wt/` 下三个独立 worktree）。
- **治理公约**：`CLAUDE.md` §1-9（归属矩阵、红线文件、路线 A/B、合并纪律、提交规范、§9 Superpowers 工作流）。
- **编排**：`.conductor/ledger.md`（4 波 16 项已 CLOSED）、`.agent/`（TASK_BOARD/PROJECT_STATE/CHANGELOG）。
- **测试源码**：12 个 QTest 测试可执行文件已写在 `motor_antomation/tests/`（单元 + 集成，含 DeviceSimulator 模拟数据管线）。

### 2.3 现状盘点（执行代理开工前先核验）
| # | 项 | 现状 | 本计划中的动作 |
|---|---|---|---|
| S1 | qmake 构建 | ✅ 可编译（有构建产物） | CI 与本地门禁重复利用 |
| S2 | CMake 测试树 | ❌ **从未真正构建通过**：缺顶层 `CMakeLists.txt`、`src/CMakeLists.txt` 模块顺序错误、无 `project()` | 已补文件 + §6 Phase 1 验证 |
| S3 | ctest 注册 | ⚠️ 写在 `tests/CMakeLists.txt`，但**从未运行过**（WI-016 明确"非目标：不涉及 CI/CD 流水线"） | Phase 1 打通并跑绿 |
| S4 | GitHub CI | ❌ 无 `.github/workflows` | Phase 3 |
| S5 | 静态分析 | ❌ 无 clang-tidy/cppcheck | Phase 4 |
| S6 | 远程备份 | ❌ **origin/master 仍停在 v0.1**，全部升级成果未 push | Phase 0 |
| S7 | 可视化看板 | ❌ 无 | Phase 5（低优先级） |
| S8 | 严格编译 `-Werror` | ⚠️ `cmake/CompilerSettings.cmake` 定义了但未启用 | 门禁绿后逐步启用 |

---

## 3. 总体架构（各环节如何协作）

```
代码修改（任意 worktree / 分支）
   │
   ├──① 本地门禁 gate.bat（agent 或人，合入前必跑）
   │      qmake 全量编译 → CMake 测试构建 → ctest 全绿
   │
   ├──② 静态检查（Superpowers requesting-code-review 环节内）
   │      clang-tidy + cppcheck → 零新增严重告警
   │
   ├──③ 提交 + push（域分支或 master）
   │      ↓
   ├──④ GitHub Actions CI（自动）
   │      装 Qt 5.14.2 mingw73_32 + MinGW 7.3.0 32bit
   │      → qmake 构建 → cmake 测试 → ctest → 红/绿
   │      ↓
   ├──⑤ GitHub Projects 看板（可视化，可选）
   │
   └──⑥ 合入 master（集成代理执行，凭④绿 + 本地验证）
```

- **agent 模拟/调试源码的两条路径**：
  - **远程路径**：push 后看 GitHub Actions 绿/红日志（无需本机环境）。
  - **本地路径**：在用户电脑用 Qt Creator 打开任一 worktree 的 `.pro` 直接编译运行（Debug 模式断点调试）；或跑 `scripts\gate.bat` 一键验证。
  - 无硬件也能行为验证：测试用 `DeviceSimulator → VofaParser → DataBus → CurveEngine` 的**模拟数据管线**（已存在于 `tests/integration/`）。

---

## 4. 需求规格（R1–R8）

> 每条含：描述、验收标准、优先级。执行代理必须逐条满足并在 §7 验收矩阵中留证据。

### R1 统一合并门禁（本地）
- 描述：提供 `scripts\gate.bat`，一键执行「qmake 全量编译 + CMake 测试构建 + ctest 全部测试」；任一步失败即退出码非 0。所有分支合入 master 前必须通过（落实 `CLAUDE.md` §4 规则 3）。
- 验收：在干净 worktree 上运行返回 0；故意引入编译错误/测试失败时返回 1 并指明失败步骤。
- 优先级：P0

### R2 ctest 注册标准化（所有 QT 项目通用）
- 描述：**用户以后所有 QT 项目的每一次代码修改**，凡涉及逻辑变更，必须配套新增/更新 QTest 用例，并用 `add_test()` 注册进 CTest。注册即"完成定义"的一部分，不注册 = 任务未完成。
- 验收：任一逻辑修改的提交中，`tests/` 下存在对应测试且 `ctest --output-on-failure` 通过；`ctest -N` 能列出全部测试。
- 优先级：P0

### R3 GitHub Actions CI（每次 push/合并自动验证）
- 描述：`.github/workflows/ci.yml` 在每次 push（任意分支）/PR/手动触发时运行：安装 Qt 5.14.2 `win32_mingw73` + MinGW 7.3.0 32 位 → qmake 构建 → CMake 构建测试 → ctest。任一失败则工作流红。
- 验收：向 GitHub push 后 Actions 自动运行并绿；故意坏提交会红且日志可定位。
- 优先级：P0

### R4 静态检查进入审查与门禁
- 描述：把 **clang-tidy + cppcheck** 接入 Superpowers 的 `requesting-code-review` 环节与合并门禁（R1 的扩展步骤）。配置 `CMAKE_EXPORT_COMPILE_COMMANDS=ON` 供 clang-tidy 使用。规则：**零新增严重/高优先级告警**（既有告警记录为基线，允许不新增）。
- 验收：提供 `scripts\static-analysis.bat`；对被测代码运行不报新增严重告警；结果写入报告文件。
- 优先级：P1

### R5 本地模拟与调试
- 描述：用户可用本机 Qt Creator 打开任意 worktree 的 `motor_antomation.pro` 编译运行（真实串口或模拟数据）；agent 用 `gate.bat` 做无界面验证。
- 验收：`motor_antomation.pro` 在 Qt Creator 中可直接构建运行；`gate.bat` 与 CI 行为一致。
- 优先级：P0

### R6 全自动化与权限边界
- 描述：除 **C 盘文件操作**（装软件、写系统目录等）需要停下请求用户人工审核外，执行代理全自动完成：仓库内文件读写、GitHub 仓库/CI/Projects 配置、构建验证、静态检查。
- 验收：任务全程唯一的人工交互发生在 C 盘操作前；其余无人工介入。
- 优先级：P0（流程约束）

### R7 可视化看板
- 描述：建立 **GitHub Projects** 看板（Todo / In Progress / Review / Done），与仓库、PR、CI 天然集成；替代 `TASK_BOARD.md` 的可视化角色（不删除原文件）。
- 验收：看板存在、仓库已连接、当前任务卡片可见。
- 优先级：P2（可选）

### R8 备份与推送
- 描述：将 `master` 与 `domain/{waveform,automation,comms}` 全部推送到 GitHub（origin 已配置 SSH）。此后每次合并后及时推送，杜绝本地唯一副本风险。
- 验收：`git ls-remote origin` 显示全部目标分支且 `master` 与本地一致。
- 优先级：P0

---

## 5. 完成定义（Definition of Done）— 本项目及未来所有 QT 项目强制标准

> 任何代码修改在声称"完成"前，必须全部满足：

| # | 检查项 | 工具/命令 | 违背后果 |
|---|---|---|---|
| D1 | 全量编译通过 | `gate.bat` 步骤 1（qmake） | 不合并 |
| D2 | 全部测试通过 | `ctest --output-on-failure` | 不合并 |
| D3 | 新逻辑有对应测试且已 `add_test()` 注册 | `ctest -N` 核对 | 视为未完成（R2） |
| D4 | 静态检查零新增严重告警 | `scripts\static-analysis.bat` | 退回审查 |
| D5 | 提交到对应分支并 push | `git push` | 视为未完成 |
| D6 | GitHub CI 绿 | Actions 页面 | 不合并 |
| D7 | UI/功能变更同步手册 | `USER_MANUAL.md` | 不合并 |
| D8 | 触发 Superpowers 审查 | `requesting-code-review` | 不合并 |

> 集成代理合并时以此表为准；机器能查的（D1/D2/D3/D4/D6）一律以机器结果为准，不采信口头"测过了"。

---

## 6. 执行计划（给执行代理的分步指令）

> 每 Phase 末尾必须报告：做了什么、验证证据（命令输出/链接）、是否满足该 Phase 验收。
> 已存在于仓库的文件（标注 ✅）不得重做，先核验再用。

### Phase 0 — 备份与推送（解决 S6）
目标：把本地全部成果推上 GitHub，建立远程唯一可信副本。
1. 在主 checkout `E:\My_project\QT\Motor_Antomation` 检查工作区：`git status`，有未提交改动先确认归属再提交。
2. 提交并推送：
   ```
   git add -A
   git commit -m "[CI] 合并门禁基础设施：顶层CMakeLists + gate.bat + ci.yml + 静态检查脚本"
   git push -u origin master
   git push origin domain/waveform domain/automation domain/comms
   ```
3. 验证：`git ls-remote origin` 列出全部四个分支。
- 验收：S6 消除；远端与本地一致。
- 人工审核点：无（不动 C 盘）。

### Phase 1 — CMake 测试树打通（解决 S2/S3）
目标：让 ctest 第一次真正跑起来。
1. 确认已有文件（✅ 本轮已创建，核验内容即可）：
   - `motor_antomation/CMakeLists.txt`（顶层：`project()`、Qt5 查找含 `Test` 组件、`include_directories(src)`、`add_subdirectory(src)`、`add_subdirectory(tests)`）
   - `motor_antomation/src/CMakeLists.txt`（13 个模块已按依赖顺序排列）
2. 在本机配置并构建测试（用用户现成工具链）：
   ```
   cmake -S motor_antomation -B build\ci-tests -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Debug -DCMAKE_PREFIX_PATH="D:/Program_Files/QT5.14/APP/5.14.2/mingw73_32" -DCMAKE_C_COMPILER="C:/MinGW/bin/gcc.exe" -DCMAKE_CXX_COMPILER="C:/MinGW/bin/g++.exe" -DCMAKE_MAKE_PROGRAM="C:/MinGW/bin/mingw32-make.exe"
   cmake --build build\ci-tests -j%NUMBER_OF_PROCESSORS%
   ```
3. 运行全部测试：
   ```
   ctest --test-dir build\ci-tests --output-on-failure
   ```
4. 预期：约 12 个测试目标全部构建成功并运行。
5. 若失败，逐个修复（常见点见 §8 风险预案 R-A）。
- 验收：`ctest` 全绿（或修复到全绿）；记录测试数与通过率。

### Phase 2 — 本地门禁跑通（落实 R1/R5）
1. 核验 `scripts\gate.bat`（✅ 已创建）：三步即 Phase 1 的 qmake 编译 + CMake 测试构建 + ctest。
2. 在仓库根 cmd 执行：`scripts\gate.bat`，确认退出码 0、三步日志完整。
3. 反向测试：临时改坏一个测试断言，确认 gate.bat 返回非 0（验证门禁真的会拦人），改回。
- 验收：R1/R5 达标；gate.bat 正向绿、反向红。

### Phase 3 — GitHub Actions 上线（落实 R3/R6/R8）
1. 确认 `.github/workflows/ci.yml`（✅ 已创建）内容：触发条件（push/pull_request/workflow_dispatch）、装 Qt 5.14.2 `win32_mingw73`、装 MinGW 7.3.0 32 位、qmake 构建、cmake 测试构建、ctest。
2. push 后到 Actions 页面观察首次运行。
3. 已知风险点：**MinGW 7.3.0 32 位安装步骤**（`aqt install-tool windows desktop tools_mingw`）。若失败，执行以下预案（按序）：
   - a. 调整 aqt 参数（指定 `-b https://download.qt.io`、`--no-modify-env` 等）重试；
   - b. 改回 `jurplel/install-qt-action@v4` 的 `tools: 'tools_mingw'` 输入；
   - c. 切换到 **self-hosted runner**（用用户本机现成工具链，最稳）：Settings → Actions → Runners → New self-hosted runner，把 `runs-on: windows-latest` 改为 runner 标签。
4. 验证：master push 后 CI 绿；提交一个故意的坏改动到临时分支确认会红，再删除该分支。
- 验收：R3 达标；CI 首次真实运行绿/红行为符合预期。

### Phase 4 — 静态分析接入（落实 R4）
1. 在 CMake 配置中启用 `-DCMAKE_EXPORT_COMPILE_COMMANDS=ON`（并入 gate/CI）。
2. 编写 `scripts\static-analysis.bat`：
   - cppcheck：`cppcheck --project=build\ci-tests\compile_commands.json --enable=warning,performance,portability --suppress=missingIncludeSystem -i build --output-file=reports\cppcheck.txt`
   - clang-tidy：`clang-tidy -p build\ci-tests $(cat compile_commands.json 中的 .cpp 文件)`
3. 先跑一遍生成**告警基线**（`reports\baseline_*.txt`），后续只要求"零新增严重告警"。
4. 把静态检查写进 `CLAUDE.md` §9：Superpowers `requesting-code-review` 之前先跑静态检查。
5. （可选，绿后）取消 `motor_antomation/CMakeLists.txt` 中 `CompilerSettings.cmake` 的注释，启用 `-Werror`。
- 验收：R4 达标；`static-analysis.bat` 可运行、有基线、无新增严重告警。

### Phase 5 — GitHub Projects 看板（落实 R7，可选）
1. 仓库 → Projects → 新建（Todo / In Progress / Review / Done 四列）。
2. 连接仓库，把当前待办（如本计划各 Phase）建成卡片。
- 验收：R7 达标（若用户不需要可跳过并在报告中注明）。

### Phase 6 — 标准化沉淀与推广
1. 把本规格的**完成定义（§5）**与**新项目接入标准（附录 B）**回写进 `CLAUDE.md`，成为项目级强制约束。
2. 沉淀可复用资产清单（见附录 A），未来新 QT 项目直接复制：`gate.bat`、`ci.yml` 模板、`static-analysis.bat`、顶层 CMakeLists 模板。
3. 全部 Phase 完成后运行 §7 验收矩阵，逐项打勾留证。
- 验收：§7 全绿；新项目接入指引可用。

---

## 7. 验收矩阵（执行代理逐项打勾并留证据）

| 编号 | 验收项 | 判定方法 | 证据位置 |
|---|---|---|---|
| A1 | R1 本地门禁 | `gate.bat` 退出码 0；反向测试非 0 | 输出日志 |
| A2 | R2 ctest 注册 | `ctest -N` 列出全部测试；新逻辑有测试 | ctest 输出 |
| A3 | R3 GitHub CI | Actions 绿；坏提交红 | Actions 页面 |
| A4 | R4 静态检查 | 脚本可跑、有基线、零新增严重告警 | reports/*.txt |
| A5 | R5 本地模拟 | Qt Creator 可构建运行；gate.bat 与 CI 一致 | 用户确认 |
| A6 | R6 权限边界 | 唯一人工交互 = C 盘操作 | 会话记录 |
| A7 | R7 看板 | Projects 存在且连接仓库 | 看板链接 |
| A8 | R8 推送备份 | `git ls-remote origin` 四分支齐全 | git 输出 |
| A9 | S2/S3 修复 | ctest 首跑全绿 | ctest 输出 |

---

## 8. 风险与预案

| 风险 | 等级 | 预案 |
|---|---|---|
| R-A：CMake 测试树有隐藏编译/链接错误（从未跑过） | 高 | 逐个修复：模块源文件清单不全 → 补进对应 `CMakeLists.txt`；缺符号 → 补 `add_library` 源文件；include 缺失 → 靠顶层 `include_directories(src)` 已兜底 |
| R-B：GitHub runner 装 MinGW 7.3.0 32 位失败 | 高 | 预案见 Phase 3 步骤 3（aqt 参数调整 → install-qt-action tools 输入 → **self-hosted runner**） |
| R-C：首跑 `-Werror` 导致大量存量告警 | 中 | 保持 `CompilerSettings.cmake` 注释状态；先跑绿，再逐步启用并把存量告警当基线清理 |
| R-D：CI 与本地 MinGW 版本行为不一致 | 中 | 以 CI 为准修正；或直接上 self-hosted runner 消除差异 |
| R-E：测试依赖 GUI/串口硬件 | 中 | 已用 `DeviceSimulator` 模拟数据管线 + `QT_QPA_PLATFORM=offscreen`；新增测试一律写无硬件依赖用例 |
| R-F：域分支长期不 merge master 导致漂移 | 中 | 沿用 `CLAUDE.md` §4：域分支定期 `git merge master`，超过 2 轮未同步即告警 |

---

## 9. 附录 A — 门禁相关文件与命令速查

| 资产 | 路径 | 说明 |
|---|---|---|
| 本地门禁 | `scripts\gate.bat` | 三步一键验证（qmake→cmake tests→ctest） |
| 静态检查 | `scripts\static-analysis.bat`（Phase 4 创建） | cppcheck + clang-tidy，输出到 `reports/` |
| CI 流水线 | `.github\workflows\ci.yml` | 每次 push/PR 自动验证 |
| 顶层 CMake | `motor_antomation\CMakeLists.txt` | 测试构建树入口 |
| 模块顺序 | `motor_antomation\src\CMakeLists.txt` | 依赖序 add_subdirectory |
| 测试注册 | `motor_antomation\tests\CMakeLists.txt` | 12 个测试目标 + add_test |
| 治理公约 | `CLAUDE.md` | §4 合并纪律 / §9 Superpowers 工作流 |

常用命令：
```
# 本地门禁
scripts\gate.bat
# 仅跑测试
ctest --test-dir build\ci-tests --output-on-failure
# 列出已注册测试
ctest --test-dir build\ci-tests -N
# 静态检查（Phase 4 后）
scripts\static-analysis.bat
# 推送全部
git push origin master domain/waveform domain/automation domain/comms
```

---

## 10. 附录 B — 新 QT 项目接入标准（用户以后所有 QT 项目通用）

新项目接入本环境，执行代理按以下清单复制/改造（预计半天内完成）：
1. 复制 `scripts/gate.bat`，把其中 `GATE_QT_DIR` / `GATE_MINGW_BIN` 默认值改成新项目工具链路径。
2. 复制 `.github/workflows/ci.yml`，修改 `working-directory` 与 `-S` 参数为新项目源码目录。
3. 为新项目源码目录补一个顶层 `CMakeLists.txt`（仿 `motor_antomation/CMakeLists.txt`），模块按依赖顺序。
4. 新项目 `tests/` 下每个测试可执行文件必须 `add_test()` 注册（R2 强制）。
5. 复制 `CLAUDE.md` 完成定义（§5）与 `scripts/static-analysis.bat`。
6. 在 GitHub 新建仓库后：`git push -u origin master`，观察首个 CI 绿。
7. 验收 = 本文件 §5 完成定义 D1-D8 全部可机器核验。

---

*文档结束。执行代理开始前请通读全文；每 Phase 完成后回到 §7 验收矩阵更新证据。*
