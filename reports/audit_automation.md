# 自动化域夜间防卡死自测 — Phase 1 只读审计缺陷清单

> 日期：2026-08-09 ｜ 审计范围：CLAUDE.md §2 domain/automation 名下 UI 控件（NodeParamPanel / FlowCanvas / NodeLibraryPanel / VariableEditorPanel / AutomationWidget）＋ 关联引擎（AutomationEngine / TestRunner / FlowRunner / ReportGenerator）
> 方法：逐控件事件处理链通读 + 4 个独立审计 lens（生命周期/场景图/编排线程/面板）交叉验证，每个发现经代码逐行确认（verified）。

## 0. 结论摘要

- 历史 BUG-001/004（`NodeParamPanel::buildForm` removeRow/takeRow 双重释放）在**当前源码中已闭环**（无 removeRow 悬垂复用），**但 BUG-004 修复引入的 `clearForm` 写法在本表单结构下不收敛**：`while (rowCount()>0){takeAt(0);}` 在含整行/空标签行时 `takeAt` 先返回 null 而 `rowCount()` 仍 > 0 → **死循环**（A-17，gdb 实证）→ **任何第二次 buildForm 即 UI 线程卡死**。这是 SYM-1/2/3「卡死」的当前活跃根因。
- 审计另发现 2 个活跃 P1（use-after-free） + 1 个 P1 线程竞态 + 多个 P4/P6：
  1. **A-17（最高危，实证）**：`clearForm` 死循环 → 每次表单重建都卡死。
  2. `NodeParamPanel::clearForm` 同步 `delete` 正在发射 `clicked()` 的发送者按钮（删除参数行 / 添加参数）→ use-after-free（两个独立 lens 确认）。
  3. `FlowCanvas::deleteSelectedItems` 在连线拖拽中删除节点 → `m_dragFromNode` 悬垂 + `m_dragLine` 残留场景 → 下一次 mouseMove 解引用已释放节点 → 崩溃。
  4. `AutomationWidget::runFlowGraph` 无重入守卫：新一轮运行在旧 worker 线程仍在执行时启动 → 两个 FlowRunner 并发调用引擎 executeStep。
- **P6 数据/UI 不同步**：`deleteSelectedItems` 删场景不删 `m_flowGraph` 数据模型 → 幻影节点被执行/被编辑。
- **功能缺口**：`VariableEditorPanel::setScope` 从未被调用 → 变量表完全失效（静默 no-op）。

## 1. 缺陷清单

| 编号 | 控件 | 根因模式 | 文件:行号 | 触发序列 | 风险 |
|---|---|---|---|---|---|
| A-01 | NodeParamPanel | **P1** use-after-free | NodeParamPanel.cpp:1121/137/147/268 | 点参数管理表删除按钮"×" → `onDeleteParam` → `buildForm` → `clearForm` 同步 `delete` paramTable（含正在发 `clicked` 的 delBtn 与打开中的单元格编辑器）→ 信号/事件返回后访问已释放内存 → 堆损坏→卡死 | 高 |
| A-02 | NodeParamPanel | **P1** use-after-free | NodeParamPanel.cpp:1160/99/147/268 | 点"+ 添加参数" → `onAddParamClicked`（QInputDialog 嵌套事件循环，发送者按钮仍存活）→ 对话框返回 → `buildForm` → `clearForm` 同步删除 addParamBtn（正在发 `clicked`）→ UAF | 高 |
| A-03 | FlowCanvas | **P1** use-after-free | FlowCanvas.cpp:1036（deleteSelectedItems）、1157（mouseMove） | 连线拖拽进行中（`m_dragFromNode` 已设）→ 经 Delete 键/右键菜单删除该节点 → `m_dragFromNode` 悬垂 + `m_dragLine` 残留 → 下一次 mouseMove `m_dragFromNode->outputPortPos()` 解引用已释放节点 → 崩溃 | 高 |
| A-04 | AutomationWidget | **P1** 线程竞态 | AutomationWidget.cpp:1068、AutomationEngine.cpp:274 | 快速连点"运行"：stop() 仅置原子标志，旧 worker 线程仍在 executeStep 时新建 FlowRunner+线程 → 两 runner 并发调引擎（无锁）→ 数据竞争 | 中 |
| A-05 | AutomationWidget | **P1** 泄漏+悬垂 | AutomationWidget.cpp:1112-1121 | 运行中销毁主窗口：worker 线程仍运行，`finished` 清理 lambda 在 `m_flowRunner` 已被重置时不再删除旧 worker/scope/runner/thread（泄漏）；worker 仍调 `ctx.engine->executeStep` 对已迁移的引擎 → 悬垂 | 中 |
| A-06 | FlowCanvas | **P6** 数据模型不同步 | FlowCanvas.cpp:1023-1067 | 删除选中节点/边 → 仅删场景项，`m_flowGraph->nodes/edges` 保留幻影 → `runFlowGraph` 执行已删节点；`blockSignals` 抑制 selectionChanged → 参数面板仍编辑幻影节点 | 中 |
| A-07 | FlowCanvas | **P4** 选择时序抖动+矢量失效隐患 | FlowCanvas.cpp:897-912 | 添加节点：`setSelected` 先于 push_back 触发 `nodeSelected(id,nullptr)`（面板 clear）→ 后 push → 显式 `nodeSelected(&back)`（面板 setNode）→ 每次添加一次 clear+build 抖动；`&nodes.back()` 在下次 push_back 后失效 | 中 |
| A-08 | FlowCanvas | **P4** 事件风暴 | FlowCanvas.cpp:442-463 | 拖动节点：每个 mouseMove 触发 `sc->update()` 全场景失效 + 遍历全部场景项 + 逐条相连边 updatePath（O(N)/次） | 低 |
| A-09 | FlowCanvas | **P4** 框选重建风暴 | FlowCanvas.cpp:1103 | RubberBand 框选拖动中每次 selectionChanged → 参数面板整表重建；仅选边也触发重建 | 低 |
| A-10 | FlowCanvas | **P6** 拖拽位置不同步 | FlowCanvas.cpp:444-447 | 拖动节点仅更新 item 的 pos，`m_flowGraph->nodes` 中 posX/posY 不更新（仅 toGraph 重读） | 低 |
| A-11 | AutomationWidget | **P5** 全量扫描 | AutomationWidget.cpp:394-407 | 参数面板每次按键 emit paramsChanged → 遍历场景所有项 × 全图节点找匹配 + 拷贝整个 FlowNode（O(scene×graph)） | 低 |
| A-12 | FlowRunner | **P4** 信号风暴 | FlowRunner.cpp:187 | 长流程运行：worker 线程高频 emit nodeStarted/nodeCompleted（约 3 事件/节点）→ UI 事件队列堆积（可达 3 万+），UI 逐条处理（每条约 O(scene) findNodeItem + centerOn）→ 卡顿 | 中 |
| A-13 | NodeParamPanel | **P1(潜在)** 裸指针入矢量 | NodeParamPanel.cpp:245 / AutomationWidget.cpp:977 | `m_currentNode` 是 `m_currentFlowGraph.nodes`（std::vector）内裸指针；`loadFlowGraph` move-assign 整体替换矢量 → 悬垂（当前被"删除即 clearNode"掩盖，属潜在 UAF） | 低 |
| A-14 | VariableEditorPanel | **P6/功能缺口** | VariableEditorPanel.cpp:143 / AutomationWidget.cpp:352 | `setScope` 全源码无调用点 → `m_scope` 恒为 null → 添加/删除/编辑变量全部静默失败 | 中 |
| A-15 | NodeParamPanel | 数据完整性 | NodeParamPanel.cpp:1136 | 参数管理表编辑"键"列 → 找不到新键则 emplace_back 重复项，旧键条目残留 | 低 |
| A-16 | TestAutomationEngine（域内测试） | **P1** 线程竞态挂起 | AutomationEngine.cpp:261-265、TestRunner.cpp:54-56 | `run()` 解锁后继续用 `const auto& testCase` 引用；`TestRunner` 析构 `BlockingQueuedConnection` 无超时 → 偶发无限挂起/失败（exit 4） | 中 |
| A-17 | NodeParamPanel | **P5** UI 线程死循环（实证 SYM 卡死根因） | NodeParamPanel.cpp:270-272 | **任何第二次 buildForm**（切换节点 setNode / 删除参数行 / 添加参数）→ `clearForm` 的 `while (rowCount() > 0) { takeAt(0); }`：当表单含整行控件（addRow(QWidget*)/addRow(QString,field)）时，QFormLayout 的 `count()` 与 `rowCount()` 失同步，`takeAt(0)` 先返回 null 而 `rowCount()` 仍 > 0 → **死循环**。gdb 实证：`clearForm` 无限循环、零 CPU 占用、`QFormLayout::takeAt: Invalid index 0` 警告洪泛 | 高 |

### 2.1 A-17 实证（Phase 2 复现用例运行发现）

用 gdb 对 `testProbeHang`（setNode ×2）抓栈，确认挂起位置：

```
#11 QFormLayout::takeAt(int)   ← "QFormLayout::takeAt: Invalid index 0" 无限循环
#12 NodeParamPanel::clearForm  NodeParamPanel.cpp:271
#13 NodeParamPanel::buildForm  NodeParamPanel.cpp:354
#14 NodeParamPanel::setNode    NodeParamPanel.cpp:255
#15 testProbeHang              test_node_param_panel.cpp:237
```

- 进程 `user 0m0.000s`（阻塞非自旋）；`takeAt: Invalid index 0` 警告持续洪泛。
- **该缺陷是 SYM-1/2/3「卡死」的真实、当前活跃根因**：任何触发表单重建的操作（切换节点、删参数行、添加参数）都会进入死循环。历史 BUG-001/004 的 `removeRow/takeRow` 双重释放是 2026-08-08 旧根因，但 **BUG-004 修复引入的 `clearForm` 写法在本表单结构下不收敛**。
- 修复方案：循环条件改为 `while (QLayoutItem* item = takeAt(0))`（以 null 为终止）；widget 用 `disconnect()+hide()+deleteLater()`（同时解决 A-01/A-02 删除信号发送者 UAF）。

## 2. SYM 症状 → 根因映射

| 症状 | 结论 | 对应缺陷/闭环 |
|---|---|---|
| SYM-1 输入→Enter→100ms 内拖动→卡死 | **A-17 clearForm 死循环（实证根因）**：输入/Enter 后任何触发重建的操作（参数增删、切换节点）进入死循环；叠加 A-01/A-02（重建删除信号发送者 UAF） | A-17/A-01/A-02/A-03 |
| SYM-2 输入后不按 Enter 点别处/拖动→偶发卡死 | **A-17**：点击别处 → 切换/清空选择 → setNode/clearNode → buildForm → clearForm 死循环；拖动期删除节点则 A-03 | A-17/A-03 |
| SYM-3 快速连续双击添加节点→卡死/崩溃 | **A-17**：每次添加 → nodeSelected → setNode → buildForm → 第二次起 clearForm 死循环；历史 BUG-001 已闭环但 A-17 使添加即卡死 | A-17/A-07 |

## 3. 已排除项（审计明确否定的假设）

- **`FlowCanvas::deleteSelectedItems` 双重释放**：原假设"同一 orphan 边被 append 两次→双重 delete"。经代码验证否定：删除节点后边端点指针为**悬垂但非空**，`!fromNode()||!toNode()` 条件恒为 false，仅"不在场景中"分支触发、每条边只 append 一次 → 无双重释放。但该段读取悬垂指针值（UB 理论），且存在 A-03（拖拽状态悬垂）与 A-06（模型不同步）。
- **paramsChanged→updateFromNode→graphChanged 信号环**：`graphChanged` 全仓库无 connect 接收者（仅 5 处 emit）→ 无 P3 信号环。实际问题是 A-11 的 O(scene×graph) 开销。
- **`m_buildingForm` 重入**：`setText` 触发 textChanged 时 `m_buildingForm` 守卫有效；两个 cellChanged lambda 均在 build 期间被 guard 拦截 → 无 build 期重入。
- **VariableEditorPanel `variablesChanged` 环**：无接收者 → 无 refreshTable 重入 UAF。

## 4. 闭环确认（BUG-001/004 根因）

`NodeParamPanel::buildForm`（NodeParamPanel.cpp:351-1168）当前实现：
- 用 `clearForm()`（takeAt+delete widget+delete item）清空，**不再** `removeRow`/`takeRow` 重插 → 无 BUG-001/004 的双重释放/悬垂复用。
- `m_buildingForm` 守卫覆盖全部类型分支的 setText/setValue/cellChanged。
- 修复前红/修复后绿的证明见 Phase 2 用例（setNode/clearNode × 50、If/Assert/SetParameter/未知类型、连续添加）。

## 5. 修复分组（同根因同方案，供 Phase 3）

| 组 | 根因 | 方案 | 覆盖缺陷 |
|---|---|---|---|
| G1 | clearForm 同步删除信号发送者（P1） | clearForm 对 widget 改用 `deleteLater()` + 先 `disconnect()`；删除后显式 `activateWindow/clearFocus` 防编辑器残留 | A-01, A-02, SYM-1/2 |
| G2 | 删除/拖拽状态生命周期（P1/P6） | deleteSelectedItems：同步删除入射边（去重）+ 同步移除数据模型节点/边 + 若删了节点 emit nodeDeselected + 若拖拽源被删 cancelEdgeDrag；itemChange 拖动同步模型位置 | A-03, A-06, A-10 |
| G3 | 添加节点选择时序（P4/P1） | addNodeFromPalette：先 push 进模型再 setSelected，去掉冗余显式 nodeSelected | A-07, A-13 |
| G4 | 引擎/面板刷新开销（P5/P4） | paramsChanged→只刷新选中项（id 直查），不加全场景×全图扫描；FlowRunner 信号按需节流 | A-11, A-12 |
| G5 | 运行重入与线程清理（P1） | runFlowGraph 加运行中守卫；析构路径保证 worker 停止与清理；TestRunner 修复解锁后引用 + BlockingQueued 超时 | A-04, A-05, A-16 |
| G6 | 变量表失效（P6/功能） | AutomationWidget 创建并绑定 VariableScope 到 VariableEditorPanel | A-14 |
| G7 | 数据完整性 | paramTable 键列编辑改为替换旧键/删旧键 | A-15 |

## 6. 需集成代理处理（红线文件）

无。全部缺陷位于 domain/automation 名下文件（`src/ui/AutomationWidget.cpp`、`src/ui/FlowCanvas.*`、`src/ui/NodeParamPanel.*`、`src/ui/VariableEditorPanel.*`、`src/automation/*`）。`tests/CMakeLists.txt` 与 `tests/` 属本任务授权修改范围。

*审计结束。下一阶段：Phase 2 复现用例（修复前红）。*
