# 波形域夜间防卡死自测 — Phase 1 只读审计缺陷清单

> 生成时间：2026-08-09 ｜ 分支：`domain/waveform`
> 审计方法：多 agent 只读审计（29 个子代理，逐文件读源码 + 对抗性验证） + 波形域 agent 直接精读核心控件。
> 覆盖范围：`src/curve/**`、`src/ui/CurveWidget`、`CurveManagerPanel`、`MultiCurveContainer`、`PlotCell`、`VerticalPlotList`、`HistoryReplayWidget`、`ChannelConfigDialog`。
> 判定标准：UI 线程单次事件处理 > 300ms 即卡死（共享规格 §2.1）。

---

## 1. 缺陷清单（13 项确认 + 1 项本域直接复现）

| # | 编号 | 控件 | 根因模式 | 文件:行号 | 风险 | 触发序列 | 状态 |
|---|---|---|---|---|---|---|---|
| 1 | WF-01 | MultiCurveContainer | **W3** | `MultiCurveContainer.cpp:461` | 高 | Grid 模式下任一 CurveWidget 右键平移 / 滚轮缩放 / 橡皮筋释放 → `notifyTimeAxisChange()` → `syncTimeAxis` 同步循环所有格子 `setTimeBase()`→`update()`，一次输入事件调度 N 个全量重绘（4x3=12 格）无节流 | 待修 |
| 2 | WF-02 | MultiCurveContainer | **W1** | `MultiCurveContainer.cpp:505` | 中 | `attachCurveEngine(engine,fps)` 把同一 engine 扇出到每个 CurveWidget，各自启动 30fps QTimer → 4x3=12 个定时器 = 360 update/sec，无跨格批处理 | 待修 |
| 3 | WF-03 | PlotCell | **P4** | `PlotCell.cpp:432` | 中 | `setChannels` 循环 `addChannel` 每加一个通道就 `rebuildChannelBar()`（deleteLater 全部 + 重建全部）→ O(N²) 控件churn + deleteLater 堆积 | 待修 |
| 4 | WF-04 | CurveManagerPanel | **P4** | `CurveManagerPanel.cpp:564` | 中 | 窗口下拉切换 → `onTargetWindowChanged` → `loadFromRegistry()` 全表重建（setRowCount(0) + 每行 5 item/2 cell widget + 每行 O(channelCount) 扫描） | 待修 |
| 5 | WF-05 | VerticalPlotList | **W3** | `VerticalPlotList.cpp:280` | 低 | 拖拽滚动每次 mouseMove 同步 `setValue()` 滚动视口并重绘，无节流 | 待修 |
| 6 | WF-06 | CurveManagerPanel | **P2** | `CurveManagerPanel.cpp:527` | 低 | 删除通道按钮槽内 `QMessageBox::question` 模态嵌套事件循环，期间 CurveWidget 拉取定时器仍 30fps 重绘 | 待修 |
| 7 | WF-07 | HistoryReplayWidget | **P5** | `HistoryReplayWidget.cpp:140` | 高 | CSV 加载在 GUI 线程同步执行完整 parse + merge + `std::sort` + 逐点 push，无 worker / 无分片 / 无进度 | 待修 |
| 8 | WF-08 | HistoryReplayWidget | **P4** | `HistoryReplayWidget.cpp:150` | 中 | 每次通道过滤切换无条件 `refreshCurveDisplay()`：clearAllChannels + 重新 merge/sort/逐点 push 全部数据 | 待修 |
| 9 | WF-09 | HistoryReplayWidget | **P2** | `HistoryReplayWidget.cpp:52` | 低 | `clearAll()` 重填通道下拉未 blockSignals：`clear()` 触发 `currentIndexChanged(-1)` → 重入 `refreshCurveDisplay()` | 待修 |
| 10 | WF-10 | CurveEngine | **P5** | `CurveEngine.cpp:251` | 中 | `downsample()` 在 GUI 线程每次 paint 都 `allPoints()` 全量复制（至多 100k 点）+ O(n) LTTB，30fps 每可见通道执行 | 待修 |
| 11 | WF-11 | TimeAxisManager | **P4** | `TimeAxisManager.cpp:48` | 中 | `updateSharedRange()` 在调用者事件内同步发射 `sharedRangeChanged` + 调用全部监听器，每个同步 CurveWidget 再 `update()` | 待修 |
| 12 | WF-12 | CurveEngine | **W1** | `CurveEngine.cpp:236` | 低 | `append()` 每数据点发射 `dataWritten`（无批处理），高频推数路径同步逐点通知 | 待修 |
| 13 | WF-13 | CurveChannel | **P5** | `CurveEngine.cpp:91` | 低 | `dataRange()` 每次拉取定时器 tick 都在 UI 线程无缓存 O(capacity) 扫描（至多 100k 点） | 待修 |
| 14 | WF-14 | CurveWidget | **W1/P4** | `CurveWidget.cpp:72` | 高 | legacy push 模式 `ch.data` 无上限增长 + `drawCurves`(L475) 每次 paint 全量绘制所有点。**已复现：600 万点单次 paint 835ms** | 待修 |

> WF-14 为波形域 agent 直接精读 + QTest 复现确认（非多 agent 审计产出）。WF-01~13 由多 agent 审计逐条验证（verdict.isReal=true）。

## 2. SYM-1 / SYM-2 本域定位结论

用户反馈的 SYM-1（输入→Enter→100ms 内拖动→卡死）、SYM-2（输入后不按 Enter 直接拖拽→偶发卡死）在自动化域的根因是 `NodeParamPanel::buildForm` 的删除按钮双重释放 + 悬垂指针（BUG-004，master 侧已修复）。

**波形域审计结论**：波形域各输入控件的 Enter/editingFinished 路径**均无「按 Enter 触发控件树重建/悬垂指针」的代码路径**（已逐条核对）：

| 输入控件 | Enter/editingFinished 路径 | 风险 |
|---|---|---|
| PlotCell `m_nameEdit`（QLineEdit） | `onNameEditingFinished`(PlotCell.cpp:257)：改 m_name、emit nameChanged、hide+show 标签。不重建控件树 | 低 |
| CurveManagerPanel QTableWidget 单元格 | `onCellChanged`(CurveManagerPanel.cpp:413)：更新 m_rows + registry。不重建控件树 | 低 |
| ChannelConfigDialog QTableWidget 单元格 | `onCellChanged`(ChannelConfigDialog.cpp:145)：更新 m_rows。不重建控件树 | 低 |

**最接近 SYM-1 的重入路径**（已列入清单）：
- WF-09：`clearAll()` 重填下拉重入 `refreshCurveDisplay()`（P2 重入）。
- WF-06：删除通道槽内模态 `QMessageBox` 期间拉取定时器持续重绘（P2 模态重入）。

**N5 要求**：无论是否发现实例，Phase 2 将为上述输入控件各写一条「键入→Enter→立即拖动」回归用例（带看门狗），作为 SYM-1/2 在波形域的专门防线。

## 3. 根因分组（Phase 3 统一修复方案）

| 组 | 根因 | 涉及条目 | 统一方案 |
|---|---|---|---|
| **G1 渲染风暴** | 输入/数据事件逐次触发全量重绘，无按帧合并/节流 | WF-01, 02, 10, 11, 13, 14 | 公共层「节流合并刷新」（脏标记 + 定时器合并 update）；downsample/dataRange 结果缓存；legacy 数据有界化 + 仅绘制可见窗口 |
| **G2 重建风暴** | 每次变更全量重建控件 | WF-03, 04, 05, 08 | 批量重建（setChannels 一次 rebuild）；变更节流；增量更新 |
| **G3 UI 线程阻塞** | 重活在 GUI 线程同步执行 | WF-07 | CSV 解析分片 + 周期性 processEvents（或后台线程）；进度反馈 |
| **G4 重入/模态** | 槽内重入 / 模态嵌套 | WF-06, 09 | blockSignals 防重入；模态期间暂停拉取重绘 |

## 4. 需集成代理处理（红线文件）

- 无。本夜波形域缺陷全部位于 domain/waveform 名下文件。

## 5. 验证标准

每条修复必须：QTest 修复前红（有证据）→ 修复后绿 → 全量 ctest 绿 → 静态检查零新增 → Phase 4 压力测试证明。
