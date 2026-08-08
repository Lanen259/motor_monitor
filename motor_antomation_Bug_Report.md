# motor_antomation Bug 解决文档（Bug Report & Fix Guide）

> **版本**: v1.0 | **日期**: 2026-08-08
> **用途**: 记录已知 Bug 的根因、复现、修复方向与验证标准，供 AI 按图修复；修复必须遵循项目规则"审查测试强制"。
> **关联**: 主设计文档 `motor_antomation_Software_Design_Document.md`（0.20.5 / 8.7.4）。

---

## 1. 如何使用本文档

1. 每个 Bug 有独立编号（BUG-XXX）、状态（待修复 / 修复中 / 已修复）、严重级别。
2. 修复一个 Bug 前，先读「根因」与「复现」，确保能稳定复现。
3. 按「修复方向」修改代码，再按「验证标准」验证（含无头复现测试 + 手动界面操作）。
4. 修复完成后必须走严格代码审查（见第 4 节），并同步 `USER_MANUAL.md`，最后 git commit。
5. 修复后在本文档把该 Bug 状态改为「已修复」并补一行「修复提交」记录。

---

## 2. Bug 列表

| 编号 | 严重 | 状态 | 简述 |
|------|------|------|------|
| BUG-001 | 严重（崩溃） | 修复不完整 | 双击添加节点 / 编辑节点参数 → `NodeParamPanel::buildForm` 双重释放 → 段错误；`2de36a5` 用 takeRow 修了 removeRow 双重释放，但"包裹段"仍有悬垂指针/堆损坏（见 BUG-004） | `2de36a5` |
| BUG-002 | 中 | 已修复 | 端口无法连线（itemAt 边界失效），见 8.6.7 |
| BUG-003 | 中 | 已修复 | FlowRunner stop/pause 空实现导致运行无法停止，见 8.6.7 |
| BUG-004 | 严重（卡死） | 待修复 | 输入参数后按回车 / 编辑 If·Assert 等节点 → buildForm"参数行包删除按钮"段悬垂指针+堆损坏 → 卡死（BUG-001 残留），见 3.7 |

---

## 3. BUG-001 详细

### 3.1 症状

- 自动化流程编辑器中，**双击节点库添加节点** → 界面卡死（实际是 `SIGSEGV` 段错误，进程崩溃/假死）
- **选中节点编辑图形化参数**（输入/下拉/通用键值表）→ 同样卡死
- 用户反馈：「可视化界面双击点击添加控件直接就卡死」「设置界面设置图形化参数卡死」（即主文档 0.20.5 / 8.7.4）

### 3.2 根因（已确认，含 gdb 调用栈）

**双重释放（double-free）**：`NodeParamPanel::buildForm()` 的「给每个参数行包一个删除按钮」逻辑中，对同一个布局项执行了两次 delete。

崩溃位置：`motor_antomation/src/ui/NodeParamPanel.cpp` **约 1114-1118 行**

```cpp
QLayoutItem* oldLabelItem = m_formLayout->itemAt(it->rowIndex, QFormLayout::LabelRole);
QLayoutItem* oldFieldItem = m_formLayout->itemAt(it->rowIndex, QFormLayout::FieldRole);
m_formLayout->removeRow(it->rowIndex);   // ← removeRow 已删除该行所有布局项和控件
delete oldLabelItem;                     // ← 双重释放 → SIGSEGV
delete oldFieldItem;                     // ← 双重释放
```

Qt 语义：`QFormLayout::removeRow(row)` **会删除该行的 label 项、field 项以及它们持有的控件**。而这里在 removeRow 之前用 `itemAt()` 取到了这两个项，removeRow 后再次 `delete` → 堆损坏 / 段错误。

**连带问题**：该段还捕获了 `it->label`（QLabel*）与 `it->field`（QWidget*）指针，在 `removeRow`（已删除它们）之后又通过 `hbox->addWidget(it->field)` 与 `insertRow(it->rowIndex, it->label, ...)` 复用了这两个已释放的指针 → 也是悬垂指针 / 二次释放风险。

gdb 调用栈：

```
Thread 1 received signal SIGSEGV, Segmentation fault.
0x0045ac87 in MotorStudio::NodeParamPanel::buildForm (this=..., node=...) at NodeParamPanel.cpp:1117
#0  NodeParamPanel::buildForm   (NodeParamPanel.cpp:1117  delete oldLabelItem;)
#1  NodeParamPanel::setNode     (NodeParamPanel.cpp:255)
#2  main ()
```

### 3.3 触发链路（为什么「双击添加节点」会崩）

1. 双击节点库 → `FlowCanvas::addNodeFromPalette(type)`
2. 新节点已通过 `m_flowGraph->nodes.push_back(node)` 同步进数据模型（`AutomationWidget` 里 `m_flowCanvas->setFlowGraph(&m_currentFlowGraph)`）
3. `item->setSelected(true)` → 选中信号 → `AutomationWidget::onFlowNodeSelected` → `m_currentFlowGraph.findNode(id)` **能找到** → `m_paramPanel->setNode(node)` → `buildForm(*node)`
4. `buildForm` 走到「参数行包删除按钮」段 → `removeRow` + `delete` → 崩溃

即：**只要 buildForm 对「带参数的节点」执行一次，就必然崩溃**。所以双击添加、点选节点编辑参数，都会触发。

### 3.4 修复方向

**核心原则：不要手动 delete `removeRow` 已经删除的项；要保留控件用于重新包装时，用 `takeRow` 而非 `removeRow`。**

- `QFormLayout::takeRow(row)` 会把该行（label 项 + field 项）从布局中取出并**返回**，**不删除**它们，由调用方接管所有权 → 才能安全地重新使用 field 控件。
- 正确写法（示意）：
  ```cpp
  auto rowItems = m_formLayout->takeRow(it->rowIndex);  // QPair<QLayoutItem*,QLayoutItem*>，不删除
  QWidget* field = rowItems.second ? rowItems.second->widget() : nullptr;
  // field 仍存活，可安全放入 container
  auto* container = new QWidget(); auto* hbox = new QHBoxLayout(container);
  hbox->addWidget(field, 1);   // field 被重新父化到 container，交给布局管理
  hbox->addWidget(delBtn);
  delete rowItems.first;       // 只需删除旧的 label 布局项（takeRow 未删除）
  m_formLayout->insertRow(it->rowIndex, it->label 或空串, container);
  ```
- **不要**复用 `it->label` 之外的已释放指针；`it->label` 若来自 takeRow 前的 itemAt 也已被接管，需谨慎。
- 备选：更简单稳妥的做法是**放弃"删行后重插"**，改为：为每个参数行直接用 `QHBoxLayout` 包裹（构造时就把 field + 删除按钮放进一个 container），避免运行期改布局结构。
- 修复后删除不再需要 `oldLabelItem/oldFieldItem` 的二次 delete。

### 3.5 复现方法（无头，供修复前后验证）

写一个最小测试（可用 build 目录里的方式链接对象）：

```cpp
NodeParamPanel panel;
FlowNode n; n.id="n1"; n.type="SetParameter";
n.params = {{"name","Speed"},{"value","1500"}};
panel.setNode(&n);          // 修复前：SIGSEGV；修复后：正常显示表单
panel.clearNode();          // 修复后：正常清空
// 再测 If 节点(QTextEdit)、未知类型(通用键值表)、连续 setNode/clearNode 多次
```

### 3.6 验证标准（通过才算修复）

1. 无头复现测试：`setNode(SetParameter)`、`setNode(If)`、`setNode(未知类型+通用键值表)`、连续 50 次 `setNode/clearNode` 均不崩溃、耗时毫秒级
2. 手动界面：双击节点库添加节点不卡死；选中节点右侧参数面板正常显示；编辑每个参数控件（输入/下拉/键值表/删除按钮/添加参数按钮）不卡死；删除某参数行后其余行正常
3. 参数编辑能写回数据模型并能保存/加载
4. 全量编译无错误，运行无崩溃

---

## 3.7 BUG-004 详细

### 3.7.1 症状

- 参数面板输入参数后**按下回车** → 界面卡死（无响应 / 假死）
- 选中 **If / Assert 等带 QTextEdit 条件表达式**的节点，或走"添加参数"对话框确认，都可能触发卡死
- 与 BUG-001 同区域（`NodeParamPanel::buildForm`），**BUG-001 的 takeRow 修复仍不彻底**，存在残留的悬垂指针/堆损坏

### 3.7.2 根因（已用无头复现 + gdb 确认）

`buildForm` 的「参数行包删除按钮」逻辑（`motor_antomation/src/ui/NodeParamPanel.cpp` 约 1086-1131 行）虽然把 `removeRow` 换成了 `takeRow`，但**对象所有权仍然错乱**，造成堆损坏：

```cpp
for (auto it = rowsToWrap.rbegin(); it != rowsToWrap.rend(); ++it) {
    auto* container = new QWidget();
    auto* hbox = new QHBoxLayout(container);
    hbox->addWidget(it->field, 1);            // (1) 把 field 移进 container（重新父化）

    auto* delBtn = new QPushButton(...);
    hbox->addWidget(delBtn);
    connect(delBtn, ..., onDeleteParam);

    QLayoutItem* oldLabelLI = itemAt(rowIndex, LabelRole);
    QLayoutItem* oldFieldLI = itemAt(rowIndex, FieldRole);
    m_formLayout->takeRow(rowIndex);          // (2) 取出该行（不删除项）

    delete oldLabelLI;                        // (3) ← 删除了 QLabel，但后面 insertRow(it->label,...) 复用它 → 悬垂指针
    delete oldFieldLI;                        // (4) ← oldFieldLI 仍持有 field（field 已在 container 里）→ 删除 field 但 container 仍引用 → 双重管理

    m_formLayout->insertRow(rowIndex, it->label, container);  // (5) 复用 (3) 已删除的 it->label
}
```

具体问题：
1. **(1)+(4)**：`hbox->addWidget(it->field)` 已把 field 重新父化进 container；`oldFieldLI`（原表单项）仍持有 field，`delete oldFieldLI` 会把 field 删掉，但 container 的布局仍引用它 → **use-after-free / 双重管理 → 堆损坏**。
2. **(3)+(5)**：`delete oldLabelLI` 删掉了 QLabel，随后 `insertRow(it->label, ...)` 又复用该**已释放指针** → 悬垂指针。
3. 堆损坏在随后访问/析构时触发崩溃或死循环（表现即"卡死"）。

gdb 证据：`Thread 1 received signal SIGTRAP ... ntdll!RtlZeroHeap`（Windows 堆损坏检测），发生在 buildForm 构建 If 节点（QTextEdit）时。

### 3.7.3 触发链路

- **按回车路径**：点"+ 添加参数" → `QInputDialog::getText` 输入参数名/值 → 按回车（默认 OK）→ `onAddParamClicked` → `buildForm(*m_currentNode)` → 包裹段堆损坏 → 卡死
- **编辑路径**：选中 If/Assert 节点（QTextEdit 表达式）→ `setNode` → `buildForm` → 包裹段堆损坏 → 卡死

### 3.7.4 修复方向（供修复 AI）

**根因是"运行期删行重插"这套逻辑本身的所有权管理错误**，推荐以下任选其一：

1. **推荐：放弃"删行后重插"**。改为在**构建参数行时就**把 field + 删除按钮放进一个 `QHBoxLayout` container（构造时组装，运行期不再改布局结构）。删除按钮按参数 key 绑定 `onDeleteParam`，删除后整体重建即可。
2. 若保留 takeRow 方式，必须正确转移所有权：
   - 先 `auto row = m_formLayout->takeRow(rowIndex);`（labelItem, fieldItem 均存活）
   - 从 fieldItem **取出 widget** 而不删除 widget（fieldItem 析构会连带删除 widget，需避免），再放入 container
   - **不要**复用任何 `delete` 过的 label 指针；label 要么一起放回，要么重建新 QLabel
3. 修复后必须按 3.6 验证标准全项通过，且专门验证：If/Assert（QTextEdit）、SetParameter（QLineEdit/QSpinBox）、未知类型（通用键值表）、连续 setNode/clearNode 50 次、添加参数对话框回车、删除参数按钮——均不崩溃、毫秒级。

---

## 4. 修复的代码审查强制要求（必须遵守）

> 依据主设计文档「项目规则」：**每次修改代码后，必须生成"代码审查"与"测试"角色（Agent）验证改动，确保万无一失；未通过审查与测试的改动不得提交。**

修复 AI 必须：

1. **先写复现**：按 3.5 复现当前崩溃，确认复现后再改。
2. **改后验证**：按 3.6 全项验证通过（含无头测试 + 手动界面）。
3. **严格审查**：让一个独立审查 Agent 检查本次改动，重点核对：
   - 布局项/控件的所有权与生命周期（是否有 removeRow/delete 双重释放、takeRow 后是否正确管理）
   - 信号连接是否造成重入（buildForm 期间控件 setText/setValue 是否触发 onAnyParamChanged）
   - 是否引入新悬垂指针 / 内存泄漏
4. **回归**：确认不影响连线、运行、加载 JSON、FlowRunner 停止/暂停等既有功能。
5. 同步 `USER_MANUAL.md`（如交互有变化），并 git commit（`[模块] 描述` 格式），提交后在本文档标记「已修复」并记录提交号。

---

## 5. 附：相关上下文

- 本次崩溃由提交 `5974032 [修复] 第八轮Bug：NodeParamPanel图形化参数编辑卡死` 引入的「参数行包删除按钮」功能导致。
- 主文档 8.7.4 / 0.20.5 记录了早期「图形化设置参数卡死」症状，其最终根因即本 BUG-001（buildForm 双重释放）。
- 复现工具：`build-motor_antomation-Desktop_Qt_5_14_2_MinGW_32_bit-Debug/` 下可用 gdb + offscreen 平台复现。
