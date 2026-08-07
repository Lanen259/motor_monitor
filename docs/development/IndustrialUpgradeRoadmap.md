# Phase 2: Industrial Upgrade Roadmap

> **Status**: 🟡 Awaiting D1/D2/D3 confirmation  
> **Date**: 2026-08-06  
> **Input**: Upgrade_Report.md (Phase 1), motor_antomation_Software_Design_Document.md (v1.3)

---

## Decision Points (Require User Confirmation)

| # | Decision | Option A | Option B | Recommendation |
|---|----------|---------|----------|----------------|
| **D1** | Toolchain | Qt 5.14.2 / C++17 / MinGW 32-bit (current) | Qt 6.6+ / C++20 / MSVC or MinGW 64-bit | **B** — design doc targets Qt6/C++20, 32-bit limits 1kHz×100ch memory |
| **D2** | Build system | qmake (.pro files, current) | CMake (top-level + presets, scalable) | **B** — cross-platform, CI-friendly, industry standard |
| **D3** | Data pipeline | ChannelManager direct (current) | DataBus pub/sub (design intent) | **B** — pub/sub enables multi-consumer, design red-line #2 |
| **D4** | Migration strategy | Big-bang: all changes at once | Incremental: P0→P1→P2→P3→P4 by priority | **B** — de-risked, each phase delivers testable increment |

---

## Phase Structure

```
P0: Architecture Backbone (Week 1-2)
├── Build unification (CMake)
├── DataBus pipeline integration  
├── Thread model v1
└── Test infrastructure

P1: Curve Engine VOFA+ Level (Week 3-4)
├── Dynamic Channel System
├── Multi-window Tab/Grid
├── Curve Manager panel
└── 1kHz performance validation

P2: Industrial UI (Week 5-6)
├── Left-nav + workspace + property + log layout
├── Dashboard cards upgrade
└── Dynamic widget system

P3: Automation (Week 7-8)
├── TestRunner full implementation
├── Flowchart editor UI
└── Report generation (HTML/CSV)

P4: Testing & Polish (Week 9-10)
├── Unit test suite (all core modules)
├── Integration tests (Mock MCU → curve)
├── CI gate (build + test + benchmark)
└── Performance profiling & optimization
```

---

## P0 — Architecture Backbone (Priority: CRITICAL)

### Goal
Make the project buildable via CMake, unify data flow through DataBus, establish thread safety baseline, and wire up testing.

### Tasks

#### T0.1: CMake Build System
- Create top-level `CMakeLists.txt` with `add_subdirectory` for all modules
- Create `CMakePresets.json` (Windows MSVC + MinGW, Linux GCC)
- Port all 14 `src/*/CMakeLists.txt` to consistent style
- Add `cmake/CompilerWarnings.cmake`
- Verify: `cmake --preset default && cmake --build` succeeds
- Deprecate `.pro` files (keep for reference, not maintenance)

#### T0.2: DataBus Pipeline Integration
- **Remove** direct `ChannelManager → CurveWidget/DashboardWidget` connect in `mainwindow.cpp`
- **Wire**: `VofaParser → DataBus::publish()` (in Comm thread via lock-free queue)
- **Wire**: `DataBus → CurveEngine::append()` (subscription)
- **Wire**: `CurveEngine → CurveWidget` (via Qt::QueuedConnection, 60Hz timer pull)
- **Wire**: `DataBus → DashboardWidget` (latest-value subscription)
- ChannelManager becomes **compatibility adapter** or is removed
- Verify: `test_phase3.cpp` modified to use DataBus path, passes

#### T0.3: Thread Model v1
- Create `CommunicationWorker` — owns SerialTransport, runs in `QThread`
- Data from Comm thread → DataBus via lock-free SPSC queue (`databus/RingBuffer.h` already exists)
- CurveEngine thread-safe append (already has mutex, verify correctness)
- UI updates via `Qt::QueuedConnection` from DataBus signals
- Graceful shutdown sequence: Comm → DataBus → UI

#### T0.4: Test Infrastructure
- Add `tests/unit/` directory mirroring `src/` structure
- Unit tests for: `VofaParser`, `RingBuffer`, `CurveEngine::CurveChannel`, `ParameterManager`, `ChannelManager`
- Port `tests/test_phase3.cpp` to CTest-registered integration test
- Add `CTest` configuration in top-level CMakeLists.txt
- Verify: `ctest` runs all tests and passes

### P0 Success Criteria
- [ ] `cmake --preset default && cmake --build` succeeds (0 errors, 0 warnings)
- [ ] Data flows: Device → Transport → Protocol → **DataBus** → CurveEngine → UI
- [ ] Comm thread isolated from UI thread (verified by thread assertion)
- [ ] `ctest` runs ≥10 unit tests, all green
- [ ] Existing features (connect, curve, dashboard, parameters, CSV export) still work

---

## P1 — Curve Engine VOFA+ Level (Priority: HIGH)

### Goal
Dynamic channel registration, multi-window curves, curve management panel, 1kHz-ready performance.

### Tasks

#### T1.1: Dynamic Channel System
- **Replace** hardcoded `TopicId` enum (`Topic.h`) with runtime `ChannelRegistry`
- Channel descriptor: `{name, unit, type(float/int), scale, offset, color, topicId}`
- `VofaParser` emits discovered channels as `ChannelDescriptor` list on first frame
- `DataBus` auto-creates topics from descriptors
- User can add custom channel via UI (name + unit + DataBus topic binding)
- Verify: Connect DeviceSimulator (8 channels) → all auto-appear in curve/dashboard

#### T1.2: Multi-Window Curve Container
- Create `CurveContainerWidget` (QWidget with QTabWidget or QGridLayout)
- Tab mode: each tab = one curve window with N channels
- Grid mode: 2×2, 1×3, configurable
- Each window = independent `CurveWidget` instance
- Each window has independent channel list and Y-axis
- Shared X-axis (time) across all windows (optional per-window override)
- "Add Window" button → creates new tab with empty curve
- "Add Channel to Window" → channel selector dialog

#### T1.3: Curve Manager Panel
- Dockable panel listing all channels
- Per-channel: color picker, name edit, unit display, Y-axis min/max, visible checkbox, delete button
- Drag channel from manager to curve window to add
- Global: "Auto Fit Y", "Pause All", "Clear All"
- Channel preset save/load (part of project JSON)

#### T1.4: Performance Optimization
- Integrate `CurveEngine::downsample()` (LTTB) into `CurveWidget::paintEvent()`
- Make `kMaxDrawPoints` configurable (default 2000, for 4K displays up to 4000)
- Remove `CurveWidget`-internal 2000-point truncation (use CurveEngine's ring buffer)
- Buffer capacity: configurable 10K-1M points per channel
- Benchmark: 1kHz × 16 channels × 60s → verify < 16ms render time, < 5% CPU
- GPU rendering path research (QOpenGLWidget as optional backend)

### P1 Success Criteria
- [ ] Connect simulator → N channels auto-register (no hardcoded names)
- [ ] Create 3+ curve windows in Tab and Grid modes
- [ ] Curve Manager: rename channel, change color, set Y-range — all reflected in real-time
- [ ] 1kHz × 8 channels × 60s: no frame drops, UI responsive
- [ ] LTTB downsampling visible in render path (profile confirms)

---

## P2 — Industrial UI (Priority: MEDIUM)

### Goal
Professional IDE-like layout, card-based dashboard, user-extensible dynamic widgets.

### Tasks

#### T2.1: Industrial MainWindow Layout
```
┌──────────┬──────────────────────┬──────────┐
│          │                      │          │
│  Nav     │    Workspace         │ Property │
│  Panel   │    (QStackedWidget)  │  Panel   │
│          │                      │          │
├──────────┴──────────────────────┴──────────┤
│              Log Console                    │
└─────────────────────────────────────────────┘
```
- Nav: Icon+text list (Dashboard, Oscilloscope, Automation, Device, Protocol, Settings)
- Workspace: QStackedWidget host for all module views
- Property: Context-sensitive (curve channel selected → channel properties; widget selected → widget properties)
- Log Console: QPlainTextEdit with colored levels, auto-scroll, filter by category

#### T2.2: Dashboard Cards Upgrade
- Card widget template: title + icon + large value + unit + sparkline (mini trend) + status indicator
- Cards: Motor Status (running/stopped/fault), Comm Status (connected/disconnected/rx/tx rate), Voltage, Current, Speed, Temperature, Fault Count
- Grid layout with drag-reorder
- Card config: which cards visible, size (small/medium/large), thresholds for color change
- Warning: yellow background flash when value near threshold
- Alarm: red background + optional sound when threshold exceeded (test mode)

#### T2.3: Dynamic Widget System
- Widget creation toolbar: Button, Slider, Input, Label, Gauge, LED Indicator
- Drag widget type to workspace → configuration dialog
- Button: label, command binding (Set Speed X, Run Script Y, Send Raw Command)
- Slider: label, min/max/step, command binding (Set Speed, Set Voltage), live preview
- Input: label, unit, command binding (Set Parameter), enter to send
- Widget layout: drag to reposition, resize
- Save/load widget layout in project JSON

### P2 Success Criteria
- [ ] Industrial layout renders: nav switches workspace, property updates contextually
- [ ] Log console shows colored, filtered log output
- [ ] Dashboard: 7+ cards with sparklines, threshold color changes work
- [ ] Create Button "High Speed Test" → binds to command → sends on click
- [ ] Create Slider "Speed" → drag → sends Set Speed command in real-time
- [ ] Widget layout persists across sessions via project JSON

---

## P3 — Automation (Priority: MEDIUM)

### Goal
Complete test execution engine with flowchart UI and report generation.

### Tasks

#### T3.1: Automation Engine Implementation
- Implement `AutomationEngine::loadTestCase()` — parse JSON test definition
- Implement `TestRunner::executeStep()` for all step types:
  - SetParameter, WaitFor, RecordData, AssertCondition, RampSpeed, SendCommand, Delay
  - Loop (with maxIterations + timeout), Branch (condition → then/else)
  - SubTest (call nested test case)
- Implement assertion engine: expression parser for conditions like `motor.speed >= 2900 && motor.speed <= 3100`
- TestContext: shared state, variable scope, device references
- Error handling: timeout per step (configurable), retry on failure (configurable)
- Dual mode: Debug (continue on failure) vs Test (stop + alarm on failure)

#### T3.2: Flowchart Editor UI
- QGraphicsView-based node editor
- Node types: Start, SetParameter, Wait, Check, Record, Stop, Branch, Loop, SubTest
- Drag nodes from palette to canvas
- Connect nodes with arrows (QGraphicsPathItem)
- Click node → property panel shows step config
- Visual feedback during execution: current node highlighted green, failed node red
- Save/load flowchart as JSON (compatible with T3.1 test definition format)

#### T3.3: Test Report Generation
- HTML report template with embedded CSS
- Sections: Summary (pass/fail count, duration), Per-Step Results (status + data + screenshot), Statistics (mean/peak/stddev), Curve Screenshots
- CSV export of raw test data
- Optional PDF (if Qt PDF module available, else HTML print)

### P3 Success Criteria
- [ ] Load JSON test case → execute all steps → report generated
- [ ] Flowchart: create 5-node sequence → save → load → execute
- [ ] Assertion: "speed >= 2900" fails when speed=2500, step marked FAIL
- [ ] Debug mode: failure doesn't stop; Test mode: failure stops + alarm
- [ ] Report HTML opens in browser with all sections populated

---

## P4 — Testing & Polish (Priority: STANDARD)

### Goal
Complete test coverage, CI integration, performance benchmarks.

### Tasks

#### T4.1: Unit Test Suite
- `tests/unit/communication/` — VofaParser (JustFloat/FireWater decode), FrameCodec (CRC, COBS)
- `tests/unit/databus/` — RingBuffer (SPSC push/pop/overflow), ChannelRingBuffer (15min window)
- `tests/unit/curve/` — CurveEngine (append/read/downsample/thread safety), LTTB accuracy
- `tests/unit/parameter/` — ParameterManager (read/write/cache/import/export/diff)
- `tests/unit/automation/` — TestRunner (each step type), assertion engine
- Target: ≥80% line coverage on core modules

#### T4.2: Integration Tests
- Mock MCU: DeviceSimulator → LoopbackTransport → VofaParser → DataBus → CurveEngine → UI
- Verify: 500Hz simulation → curve displays correct waveforms (assert pixel-level or data-level)
- Verify: Fault injection → FaultWidget shows correct fault
- Verify: Parameter write → round-trip (write via UI, read back, assert equality)

#### T4.3: CI/CD Pipeline
- `.github/workflows/ci.yml`: Windows (MSVC) + Linux (GCC) build matrix
- Steps: configure → build → test → benchmark
- Performance regression: fail if `test_phase3` throughput drops >10% vs baseline
- Artifact: Windows portable zip, Linux AppImage

### P4 Success Criteria
- [ ] `ctest` runs ≥30 tests, all green, on both Windows and Linux
- [ ] Integration test: DeviceSimulator → curve displays 8 channels correctly
- [ ] CI: PR triggers build+test, passes within 5 minutes
- [ ] Performance: 1kHz × 16 channels sustained for 10 minutes without degradation

---

## Risk Register

| Risk | Impact | Likelihood | Mitigation |
|------|--------|------------|------------|
| Qt5→Qt6 migration breaks existing code | High | Medium | Keep Qt5 compat layer; CI tests both |
| DataBus pub/sub performance at 1kHz×100ch | High | Low | Benchmark early in P0; fallback to direct ring buffer |
| CMake complexity delays P0 | Medium | Medium | Reuse existing `src/*/CMakeLists.txt` structure; only add top-level |
| CurveWidget QPainter too slow at 1kHz | Medium | Medium | Implement QOpenGLWidget backend as fallback |
| Automation engine scope creep | Medium | High | Strict MVP: 8 step types max in v1; flowchart is read-only visual |
| Thread bugs (race/deadlock) | High | Medium | ThreadSanitizer in CI; stress test in P4 |

---

## Timeline Estimate

| Phase | Duration | Cumulative |
|-------|----------|------------|
| P0 | 2 weeks | Week 2 |
| P1 | 2 weeks | Week 4 |
| P2 | 2 weeks | Week 6 |
| P3 | 2 weeks | Week 8 |
| P4 | 2 weeks | Week 10 |

**Total: 10 weeks to industrial-grade v1.0**

---

> **Next Step**: Confirm D1/D2/D3/D4 → Begin P0 implementation.
