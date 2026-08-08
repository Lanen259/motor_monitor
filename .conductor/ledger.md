# Conductor Ledger — Motor Automation Industrial Upgrade
> Updated: 2026-08-06 | Mode: deep | Phase: CLOSED (16/16 complete)

## Run Contract
- **Goal**: Upgrade from v0.1 engineering demo → industrial-grade Motor Automation Studio
- **Deliverables**: DataBus-centric pipeline, VOFA+ curve system, automation engine, industrial UI, test suite
- **Constraints**: Qt 5.14.2 / C++17 / qmake / MinGW 32-bit (keep current env per D1/D2)
- **Success**: All A1-A11 acceptance criteria from Upgrade_Report.md met
- **Rework limit**: 2 rounds per worker
- **Mode**: deep
- **Final status**: ALL COMPLETE — 4 waves, 16 work items, 0 rework cycles needed

## Decision Log
| # | Decision | Status |
|---|----------|--------|
| D1 | Keep Qt 5.14.2 / C++17 / qmake / MinGW 32-bit | Confirmed |
| D2 | qmake primary, CMake deferred | Confirmed |
| D3 | DataBus+CurveEngine as sole data channel | Confirmed |

## Dependency Graph
```
Wave 1 (parallel, no inter-deps):
  WI-005 (P1-01 Dynamic Channel) ──────── dispatched
  WI-006 (P2-01 Industrial UI) ────────── dispatched
  WI-007 (P3-01 Automation Engine) ────── dispatched

Wave 2 (after Wave 1, parallel where deps met):
  WI-008 (P1-02 Multi-Window Curve) ──── blocked by WI-005
  WI-011 (P2-02 Dashboard Cards) ──────── blocked by WI-006
  WI-012 (P2-03 Dynamic Widget) ───────── blocked by WI-006
  WI-013 (P3-02 Flowchart UI) ─────────── blocked by WI-007
  WI-014 (P3-03 Test Report) ──────────── blocked by WI-007
  WI-015 (P4-01 Integration Tests) ────── ready (P0-02/P0-04 done)
  WI-016 (P4-02 CI Gate) ──────────────── ready (P0-04 done)

Wave 3 (after Wave 2):
  WI-009 (P1-03 Curve Manager) ────────── blocked by WI-005, WI-008
  WI-010 (P1-04 High-Freq Perf) ───────── blocked by P0-03, WI-008
```

## Work Items

### P0-01: Build System Unification
- Status: skipped (per D1/D2, keeping qmake)
- Note: CMake migration deferred to future Qt6 upgrade

### P0-02: Data Pipeline Unification ✅
- Status: completed
- Assignee: conductor
- Acceptance: DataBus is sole data channel; CurveEngine stores data; CurveWidget pulls from CurveEngine; DashboardWidget polls DataBus
- Files: Topic.h/cpp, DataBus.h/cpp, CurveWidget.h/cpp, DashboardWidget.h/cpp, mainwindow.h/cpp

### P0-03: Thread Model v1 ✅
- Status: completed
- Assignee: conductor
- Acceptance: Comm thread → DataBus (thread-safe); UI updates via QueuedConnection
- Files: DeviceWorker.h/cpp (new), mainwindow.h/cpp (updated)
- Note: SerialTransport+VofaParser on dedicated QThread; cross-thread via Qt signals

### P0-04: Test Infrastructure ✅
- Status: completed
- Assignee: conductor
- Acceptance: Unit tests for VofaParser/CurveEngine/DataBus/TopicRegistry; test_phase3 ported
- Files: tests/unit/communication/test_vofa_parser.cpp, tests/unit/databus/test_databus.cpp, tests/unit/databus/test_topic_registry.cpp, tests/unit/curve/test_curve_engine.cpp

### WI-005: Dynamic Channel System (P1-01) ✅
- Status: completed (Wave 1)
- Assignee: worker (agent)
- File: .conductor/work/WI-005/work-order.md
- Depends: P0-02 ✅
- Note: ChannelDescriptor struct, TopicRegistry enhanced, ChannelConfigDialog with 6-column edit, CurveWidget+Dashboard auto-sync descriptors

### WI-006: Industrial UI Layout (P2-01) ✅
- Status: completed (Wave 1)
- Assignee: worker (agent)
- File: .conductor/work/WI-006/work-order.md
- Depends: —
- Note: 4-zone industrial layout (left nav + QStackedWidget workspace + right property dock + bottom log console)

### WI-007: Automation Engine (P3-01) ✅
- Status: completed (Wave 1)
- Assignee: worker (agent)
- File: .conductor/work/WI-007/work-order.md
- Depends: P0-02 ✅
- Note: Full engine with 8 step types, TestRunner async, 20 unit tests, sample JSON, Q_DECLARE_METATYPE for QSignalSpy

### WI-008: Multi-Window Curve System (P1-02) ✅
- Status: completed (Wave 2)
- File: .conductor/work/WI-008/work-order.md
- Depends: WI-005 ✅
- Note: MultiCurveContainer with Tab (add/close/rename) + Grid (2x2/3x2/4x3 with sync'd time axis)

### WI-009: Curve Manager Panel (P1-03) ✅
- Status: completed (Wave 3)
- File: .conductor/work/WI-009/work-order.md
- Depends: WI-005 ✅, WI-008 ✅
- Note: QTableWidget 7-col editor, QColorDialog, visibility toggle, delete from all layers, embedded in Oscilloscope QSplitter

### WI-010: High-Frequency Performance (P1-04) ✅
- Status: completed (Wave 3)
- File: .conductor/work/WI-010/work-order.md
- Depends: P0-03, WI-008 ✅
- Note: LTTB in drawCurves(), 100k default capacity, setCapacity(), frame timing, dual-mutex thread safety, 3 new tests

### WI-011: Dashboard Cards Upgrade (P2-02) ✅
- Status: completed (Wave 2)
- File: .conductor/work/WI-011/work-order.md
- Depends: WI-006 ✅
- Note: DashboardCard QFrame subclass, 7 card types, 3-level thresholds, adaptive font scaling

### WI-012: Dynamic Widget System (P2-03) ✅
- Status: completed (Wave 3)
- File: .conductor/work/WI-012/work-order.md
- Depends: WI-006 ✅
- Note: DynamicWidgetFactory (button/slider/input), WidgetBindingManager with {value} substitution, toolbar "+" creation dialog

### WI-013: Flowchart UI (P3-02) ✅
- Status: completed (Wave 2)
- File: .conductor/work/WI-013/work-order.md
- Depends: WI-007 ✅
- Note: AutomationWidget with QTableWidget step list, detail panel, execution log, Run/Pause/Stop

### WI-014: Test Report Generation (P3-03) 🔵
- Status: dispatched (Wave 4)
- File: .conductor/work/WI-014/work-order.md
- Depends: WI-007 ✅

### WI-015: Integration Tests (P4-01) 🔵
- Status: dispatched (Wave 4)
- File: .conductor/work/WI-015/work-order.md
- Depends: P0-04 ✅, P0-02 ✅

### WI-016: CI Gate (P4-02) 🔵
- Status: dispatched (Wave 4)
- File: .conductor/work/WI-016/work-order.md
- Depends: P0-04 ✅

---

## Worktree Topology (2026-08-08)
> 多 agent 并行开发的物理隔离层。每个 domain 分支有独立 checkout（git worktree），agent 只允许改自己名下的文件。归属矩阵基于 5 路耦合度分析（域间零依赖、UI 按域干净切分、mainwindow 为唯一组合根）。

| Worktree path | Branch | Owner |
|---|---|---|
| E:/My_project/QT/Motor_Antomation (main) | master | 集成代理 (shell + hub + docs + build) |
| E:/My_project/QT/Motor_Antomation-wt/waveform | domain/waveform | waveform agent |
| E:/My_project/QT/Motor_Antomation-wt/automation | domain/automation | automation agent |
| E:/My_project/QT/Motor_Antomation-wt/comms | domain/comms | comms agent |

### master-owned（只在 master 上改；域 worktree 一律只读，改了必打架）
- motor_antomation/mainwindow.h/.cpp/.ui, main.cpp
- src/ui/DynamicWidgetFactory.*, src/ui/WidgetBindingManager.*
- src/core/**（Message.h 的 DataPoint 是总线 ABI）、src/databus/**（DataBus/Topic.h/TopicRegistry/RingBuffer/ChannelManager）
- src/logging/**、src/app/**
- 根目录 *.md、*.pro、docs/、构建产物配置

### domain/waveform 名下
- src/curve/**
- src/ui/CurveWidget、CurveManagerPanel、MultiCurveContainer、PlotCell、VerticalPlotList、HistoryReplayWidget、ChannelConfigDialog

### domain/automation 名下
- src/automation/**、src/parameter/**、src/scripting/**、src/plugin/**
- src/ui/AutomationWidget、FlowCanvas、NodeLibraryPanel、NodeParamPanel、VariableEditorPanel

### domain/comms 名下
- src/communication/**、src/device/**、src/data/**
- src/ui/DashboardWidget、FaultWidget

### 合并纪律
1. 域 worktree 只改自己名下文件；需要动 master-owned 文件（如新增 TopicId、改 DataPoint、接 mainwindow）→ 停下，交给集成代理在 master 上做，再 merge 回域分支。
2. 每个域分支定期 `git merge master`，别跑偏超过 1~2 轮。
3. 域分支合回 master 前必须：编译通过 + 单元/集成测试通过 + 冒烟。
4. 冲突理论上只会出现在 master-owned 文件 → 由集成代理统一裁决。
