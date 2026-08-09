# Motor_Antomation — 项目治理公约

> Qt 5.14 / C++17 / qmake / MinGW 32-bit 电机自动化监控平台。
> 本文件是所有 agent 的强制约束：进入本仓库的每个会话 / 子代理都会自动加载它。开工前必读。

---

## 1. 仓库拓扑（worktree 一览）

仓库用 **git worktree** 做了多分支物理隔离。每个 worktree 都是完整代码副本（qmake 递归 glob 收源文件），各自独立编译、独立 build 目录，互不污染。

| 分支 | worktree 路径 | 归属 | 角色 |
|---|---|---|---|
| **master** | `E:/My_project/QT/Motor_Antomation`（主 checkout） | 集成代理 | 集成基线 + 壳层/中枢/文档的开发地 |
| **domain/waveform** | `Motor_Antomation-wt/waveform` | 波形代理 | 波形/曲线域开发 |
| **domain/automation** | `Motor_Antomation-wt/automation` | 自动化代理 | 自动化/参数/脚本/插件域开发 |
| **domain/comms** | `Motor_Antomation-wt/comms` | 通讯代理 | 通讯/设备/数据域开发 |

> `Motor_Antomation-wt` 不是一个整体，是**三个互相独立的开发工作区**，对应三条域分支。

---

## 2. 文件归属矩阵（谁拥有谁）

| 归属 | 名下文件 |
|---|---|
| **master-owned** | `motor_antomation/mainwindow.h/.cpp/.ui`、`main.cpp`；`src/ui/DynamicWidgetFactory.*`、`src/ui/WidgetBindingManager.*`；`src/core/**`、`src/databus/**`、`src/logging/**`、`src/app/**`；根目录全部 `*.md`、`*.pro`、`docs/`、构建配置 |
| **domain/waveform** | `src/curve/**`；`src/ui/CurveWidget`、`CurveManagerPanel`、`MultiCurveContainer`、`PlotCell`、`VerticalPlotList`、`HistoryReplayWidget`、`ChannelConfigDialog` |
| **domain/automation** | `src/automation/**`、`src/parameter/**`、`src/scripting/**`、`src/plugin/**`；`src/ui/AutomationWidget`、`FlowCanvas`、`NodeLibraryPanel`、`NodeParamPanel`、`VariableEditorPanel` |
| **domain/comms** | `src/communication/**`、`src/device/**`、`src/data/**`；`src/ui/DashboardWidget`、`FaultWidget` |

**红线文件（master-owned 中最高危，域 worktree 一律只读）：**
- `mainwindow.h/.cpp/.ui` — 唯一组合根，所有域在它这里汇合
- `src/databus/Topic.h` — TopicId 是跨域 ABI，automation/curve/UI 都读它
- `src/core/message/Message.h` — `DataPoint` 是总线 ABI

---

## 3. 两条开发路线（核心判断规则）

**路线 A — 域开发：在对应 worktree 里做。**
波形→`wt/waveform`、自动化→`wt/automation`、通讯→`wt/comms`。改完在该 worktree 提交到域分支。

**路线 B — 壳层 / 中枢 / 文档 / 构建：直接在主 checkout（master）里改。**
`mainwindow.*`、`src/core/**`、`src/databus/**`、`src/logging/**`、`src/app/**`、`src/ui/DynamicWidgetFactory`、`WidgetBindingManager`、所有 `*.md`、`*.pro`。不经过任何 worktree，直接在 master 上开发、提交。

> **master 不是"只读成品仓库"，它是一个活跃开发分支**：壳层和中枢的日常开发就在它上面发生。只是域功能不在 master 上直接写，必须走域分支。

### 判断步骤（任务到手后按序执行）

1. 列出该任务需要改动的**全部文件**。
2. 全部落在某域名下 → **路线 A**，进对应 worktree。
3. 全部落在 master 名下 → **路线 B**，直接在主 checkout 改。
4. **混合任务**（域文件 + 壳层文件都要动）→ 拆分：
   - 域部分 → 域 worktree；
   - `mainwindow` 等接线部分 → **报给集成代理在 master 上做**（域代理绝不直接在 worktree 里改 master-owned 文件）。
   - 若接线改动很小、域部分占比很低 → 整个任务作为壳层任务直接在 master 做，但要在提交说明里注明涉及哪些域。

**典型混合任务示例**：自动化代理要给工具栏加按钮 → 自动化域部分在 `wt/automation` 完成，`mainwindow` 的接线交给集成代理在 master 接；两边各自提交，由集成代理统一集成验证。

---

## 4. 合并纪律与方向

**合并动作只有两种箭头，方向不可逆：**

```
     定期同步（把 master 拉进域分支，防跑偏）
     ◄────────────────────────────────────────
     domain/*  ───────────────────────────────►  master
     功能完成且验证通过后（push 进 master）
```

1. **master → 域分支（pull）**：域分支定期 `git merge master`，别跑偏超过 1~2 轮。此动作在域 worktree 内执行。
2. **域分支 → master（merge）**：功能完成 + 全部验证通过后执行。**此动作永远在主 checkout（`Motor_Antomation`）执行**，因为 master 被主 checkout 锁定。
3. 合并前置门禁（缺一不可）：**编译通过 + 单元测试通过 + 集成测试通过 + 手动冒烟**。
4. 冲突理论上只会出现在 master-owned 文件 → 由集成代理统一裁决，域代理不得自行改写 master-owned 文件来"解决冲突"。
5. 合入 master 后，若涉及 UI/功能变化 → 同步 `USER_MANUAL.md`（见 §6）。

---

## 5. 开发流程

### 路线 A — 域任务完整流程
```bash
cd E:/My_project/QT/Motor_Antomation-wt/waveform   # 或 automation / comms
git merge master                # 1. 先同步 master 最新
# 2. 只改自己名下文件（见 §2）
# 3. 编译 + 单元/集成测试 + 冒烟
git add <名单> && git commit    # 4. 提交到域分支（见 §7 提交规范）
# 5. 功能完成后：
cd E:/My_project/QT/Motor_Antomation
git merge domain/waveform       # 6. 合进 master（集成代理执行 + 验证）
```

### 路线 B — 壳层任务完整流程
```bash
cd E:/My_project/QT/Motor_Antomation               # 主 checkout
git add <名单> && git commit     # 直接改、直接提交到 master
```

### 混合任务流程
域部分走路线 A 流程（步骤 1-4）；接线需求上报集成代理，集成代理在 master 完成接线，双方各自提交后统一集成验证。

---

## 6. 提交与同步规范

1. **每个 worktree / 分支的改动独立提交**，不要跨 worktree 混提。
2. 提交信息用现有前缀约定：`[波形]`、`[自动化]`、`[通讯]`、`[SHELL]`、`[修复]`、`[DOC]`、`[v0.x]`、`[文档]` 等，保持与历史一致。
3. **自动提交**：每次变更会话结束后提交本次改动，不留脏工作区。
4. 不提交构建产物（`build*/` 已被 `.gitignore` 排除）。
5. **手册同步规则**：每次 UI / 功能变更后，`USER_MANUAL.md` 必须在 master 上同步更新（域分支内不单独改手册）。

---

## 7. Bug 修复流程

1. 发现 bug → 在 master 上记 `BUG-00x`（`motor_antomation_Bug_Report.md`）。
2. 从 master 切 `hotfix/bug-00x` 分支修复（若 bug 只在某域，可先在对应域分支修复再合回）。
3. 编译 + 测试验证通过后，`hotfix/bug-00x` 合回 master。
4. Bug 文档与修复提交号同步记录在 master。

---

## 8. 职责边界速查（谁负责什么）

| 动作 | 执行者 |
|---|---|
| 改域文件、域分支提交 | 对应域代理 |
| 改 `mainwindow` / `core` / `databus` / 文档 / 构建 | 集成代理（master） |
| 域分支定期 pull master | 对应域代理 |
| 域分支合回 master + 集成验证 | 集成代理（主 checkout） |
| 冲突裁决 | 集成代理 |
| 新增 TopicId / 改 DataPoint / 接 mainwindow | 集成代理（master），域代理申请 |
| 手册 / Bug / 设计文档同步 | 集成代理（master） |

完整归属矩阵与合并纪律同源备份在 `.conductor/ledger.md` 的 "Worktree Topology" 一节。

---

## 9. 工作方式（Superpowers）

本项目使用 Superpowers 工作流。开始任何任务前，先判断并调用相关技能，不要直接动手：

- 新功能 / 行为改动 → brainstorming（先澄清需求）→ writing-plans
- Bug 修复 → systematic-debugging
- 写代码 → test-driven-development
- 多任务计划执行 → subagent-driven-development 或 dispatching-parallel-agents
- 代码完成 → 先跑静态检查 `scripts\static-analysis.bat`（零新增严重告警，基线 `reports/baseline_*.txt`），再 requesting-code-review；收到意见 → receiving-code-review
- 宣称完成前 → verification-before-completion（机器证据优先：`gate.bat` + `ctest` + CI 绿）

**优先级：本文件的指令 > 技能 > 默认行为。除非用户明确说跳过技能，否则不要跳过。**

### 9.1 每次代码修改 = 强制完整流程（不可跳过）

**任何代码修改**（新增功能、修 Bug、重构、UI 调整，无论大小）**必须走完整 Superpowers 工作流**，禁止"改完就交"：

1. **澄清与计划**：新功能先 `brainstorming` → `writing-plans`；修 Bug 先 `systematic-debugging`；动手前明确需求。
2. **TDD 实现**：先写/更新 QTest 用例再写实现；新逻辑必须有对应测试并 `add_test()` 注册进 CTest（见 §10 D3）。
3. **静态检查**：`scripts\static-analysis.bat` 零新增严重告警（D4）。
4. **代码审查（强制）**：实现完成后必须 `requesting-code-review`，收到意见后 `receiving-code-review` 逐条处理。**禁止跳过、禁止假装走流程。**
5. **机器验证**：`gate.bat` 全量编译 + `ctest --output-on-failure` 全绿 + GitHub CI 绿（D1/D2/D6）。
6. **收尾**：UI/功能变更同步 `USER_MANUAL.md`（D7），`git commit` 并 push（D5）。

**审查红线（审查 Agent 必须逐项核对，缺一不可）：**
- **正确性**：内存/生命周期、悬垂指针、信号重入、线程安全、异常路径。
- **完整性（不偷工减料）**：界面控件与交互必须做成**完整可用功能**，禁止占位/示例级/半成品/降级糊弄；功能参数、边界条件、失败处理都要覆盖。
- **测试与回归**：有测试、能回归，不破坏既有功能。

**违背后果**：任一环节被跳过 = 改动视为未完成，禁止提交与合并（与 §10 D8 同源）。

---

## 10. 完成定义（Definition of Done，本项目强制标准）

> 任何代码修改在声称"完成"前，必须全部满足（与 `docs/QT自动化开发验证环境_需求与执行规格.md` §5 同源）：

| # | 检查项 | 工具/命令 | 违背后果 |
|---|---|---|---|
| D1 | 全量编译通过 | `scripts\gate.bat` 步骤 1（qmake） | 不合并 |
| D2 | 全部测试通过 | `ctest --output-on-failure` | 不合并 |
| D3 | 新逻辑有对应 QTest 用例且已 `add_test()` 注册 | `ctest -N` 核对 | 视为未完成 |
| D4 | 静态检查零新增严重告警 | `scripts\static-analysis.bat` | 退回审查 |
| D5 | 提交到对应分支并 push | `git push` | 视为未完成 |
| D6 | GitHub CI 绿 | Actions 页面（`.github/workflows/ci.yml`） | 不合并 |
| D7 | UI/功能变更同步手册 | `USER_MANUAL.md` | 不合并 |
| D8 | 触发 Superpowers 审查 | `requesting-code-review` | 不合并 |

> 集成代理合并时以此表为准；机器能查的（D1/D2/D3/D4/D6）一律以机器结果为准，不采信口头"测过了"。

## 11. 新 QT 项目接入标准（所有未来 QT 项目通用）

新项目接入本验证环境（预计半天内完成）：
1. 复制 `scripts/gate.bat`，把 `GATE_QT_DIR` / `GATE_MINGW_BIN` 默认值改成新项目工具链路径。
2. 复制 `.github/workflows/ci.yml`，修改 `working-directory` 与 `-S` 参数为新项目源码目录。
3. 为新项目源码目录补一个顶层 `CMakeLists.txt`（仿 `motor_antomation/CMakeLists.txt`），模块按依赖顺序。
4. `tests/` 下每个测试可执行文件必须 `add_test()` 注册（强制）。
5. 复制本文件 §10 完成定义与 `scripts/static-analysis.bat`。
6. GitHub 新建仓库后：`git push -u origin master`，观察首个 CI 绿。
7. 验收 = §10 的 D1-D8 全部可机器核验。
