# Motor_Antomation — 项目公约

Qt 5.14 / C++17 / qmake / MinGW 32-bit 电机自动化监控平台。

## 多 agent 并行开发公约（开工前必读）

仓库用 **git worktree** 做了多分支物理隔离。任何 agent 动手前，先判断任务归属域，只允许改自己名下的文件。

| 分支 / worktree 路径 | 归属 | 名下文件 |
|---|---|---|
| **master**（`E:/My_project/QT/Motor_Antomation`） | 集成代理 | `mainwindow.h/.cpp/.ui`、`main.cpp`、`src/core/**`、`src/databus/**`、`src/logging/**`、`src/app/**`、`src/ui/DynamicWidgetFactory.*`、`src/ui/WidgetBindingManager.*`、全部 `*.md` / `*.pro` / `docs/` |
| **domain/waveform**（`Motor_Antomation-wt/waveform`） | 波形代理 | `src/curve/**`；`src/ui/CurveWidget`、`CurveManagerPanel`、`MultiCurveContainer`、`PlotCell`、`VerticalPlotList`、`HistoryReplayWidget`、`ChannelConfigDialog` |
| **domain/automation**（`Motor_Antomation-wt/automation`） | 自动化代理 | `src/automation/**`、`src/parameter/**`、`src/scripting/**`、`src/plugin/**`；`src/ui/AutomationWidget`、`FlowCanvas`、`NodeLibraryPanel`、`NodeParamPanel`、`VariableEditorPanel` |
| **domain/comms**（`Motor_Antomation-wt/comms`） | 通讯代理 | `src/communication/**`、`src/device/**`、`src/data/**`；`src/ui/DashboardWidget`、`FaultWidget` |

每个 worktree 都是完整代码副本（qmake 递归 glob 收源文件），各自独立编译、独立 build 目录，互不污染。

### 铁律（违反必打架）

1. **只改自己名下的文件。** master-owned 文件（尤其 `mainwindow.h/.cpp`、`src/databus/Topic.h` 的 TopicId、`src/core/message/Message.h` 的 DataPoint）对域 worktree **只读**；确实需要改 → 停下，报告集成代理在 master 上做，再 merge 回域分支。
2. 域分支定期 `git merge master`，别跑偏超过 1~2 轮。
3. 域分支合回 master 前必须：编译通过 + 单元/集成测试通过 + 冒烟。
4. 文档（`USER_MANUAL.md`、Bug/设计文档）只在 master 上同步。

完整归属矩阵与合并纪律见 `.conductor/ledger.md` 的 "Worktree Topology" 一节。
