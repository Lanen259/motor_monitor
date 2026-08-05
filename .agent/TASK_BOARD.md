# TASK_BOARD.md — Motor Automation 任务看板

> 最后更新: 2026-08-05T20:35+08:00
> 当前版本: v0.1
> 构建系统: qmake (Qt 5.14.2 MinGW 7.3.0 32-bit)

---

## 看板说明

- 🔵 Todo — 待开发
- 🟡 In Progress — 开发中
- 🟢 Done — 已完成（编译通过）

---

## v0.1: 基础调试

### 模块: SHELL — 主框架 🟢 完成

| ID | 任务 | 状态 |
|----|------|------|
| SHELL-01 | MainWindow 主框架（菜单栏+工具栏+状态栏） | 🟢 Done |
| SHELL-02 | 多页面容器（QTabWidget，可关闭/可移动） | 🟢 Done |
| SHELL-03 | 串口选择/波特率选择/连接按钮 | 🟢 Done |
| SHELL-04 | 窗口状态持久化 + 关于对话框 | 🟢 Done |

### 模块: SERIAL — 串口通讯 🟡 开发中

| ID | 任务 | 状态 | Agent | 依赖 |
|----|------|------|-------|------|
| SERIAL-01 | 串口扫描与枚举 | 🔵 Todo | — | — |
| SERIAL-02 | 串口连接/断开管理 | 🔵 Todo | SERIAL-01 | — |
| SERIAL-03 | 波特率/数据位/停止位配置 | 🔵 Todo | SERIAL-02 | — |
| SERIAL-04 | 断线检测与重连提示 | 🔵 Todo | SERIAL-02 | — |

### 模块: 协议解析 (PROTOCOL)

| ID | 任务 | 状态 | Agent | 依赖 |
|----|------|------|-------|------|
| PROTO-01 | VOFA+ JustFloat 协议解析引擎 | 🔵 Todo | — | SERIAL-02 |
| PROTO-02 | 帧头/帧尾/CRC 校验 | 🔵 Todo | PROTO-01 | — |
| PROTO-03 | 多通道变量自动映射 | 🔵 Todo | PROTO-01 | — |
| PROTO-04 | 命令帧封装（启停/设定值/参数读写等） | 🔵 Todo | PROTO-01 | — |

### 模块: 实时数据总线 (DATABUS)

| ID | 任务 | 状态 | Agent | 依赖 |
|----|------|------|-------|------|
| DATA-01 | 实时数据环形缓冲区（15分钟） | 🔵 Todo | — | PROTO-01 |
| DATA-02 | 数据订阅/发布机制（EventBus） | 🔵 Todo | — | — |
| DATA-03 | 降采样引擎（高帧率→60Hz显示） | 🔵 Todo | DATA-01 | — |

### 模块: 曲线波形 (CURVE)

| ID | 任务 | 状态 | Agent | 依赖 |
|----|------|------|-------|------|
| CURVE-01 | 实时曲线渲染（Qt Quick / OpenGL） | 🔵 Todo | — | DATA-02 |
| CURVE-02 | 多通道配色与图例 | 🔵 Todo | CURVE-01 | — |
| CURVE-03 | 缩放/平移交互 | 🔵 Todo | CURVE-01 | — |
| CURVE-04 | 峰值/谷值/均值自动标注 | 🔵 Todo | DATA-03 | — |

### 模块: 仪表盘 (DASHBOARD)

| ID | 任务 | 状态 | Agent | 依赖 |
|----|------|------|-------|------|
| DASH-01 | 数值显示组件（电流/电压/转速/温度） | 🔵 Todo | — | DATA-02 |
| DASH-02 | 多页面布局管理（Tab/分页） | 🔵 Todo | — | — |
| DASH-03 | 页面创建/删除/命名 | 🔵 Todo | DASH-02 | — |

### 模块: 参数系统 (PARAM)

| ID | 任务 | 状态 | Agent | 依赖 |
|----|------|------|-------|------|
| PARAM-01 | 参数表格视图（名称/值/类型/范围） | 🔵 Todo | — | PROTO-04 |
| PARAM-02 | 参数读取（单条/批量） | 🔵 Todo | PROTO-04 | — |
| PARAM-03 | 参数写入（单条/批量） | 🔵 Todo | PROTO-04 | — |
| PARAM-04 | 参数导出JSON | 🔵 Todo | — | — |

### 模块: 电机控制 (CONTROL)

| ID | 任务 | 状态 | Agent | 依赖 |
|----|------|------|-------|------|
| CTRL-01 | 启停/正反转按钮 | 🔵 Todo | — | PROTO-04 |
| CTRL-02 | 转速/电流/位置设定 | 🔵 Todo | PROTO-04 | — |
| CTRL-03 | 控制模式切换（电流/速度/位置） | 🔵 Todo | PROTO-04 | — |
| CTRL-04 | 键盘快捷键（方向键调速/空格启停） | 🔵 Todo | CTRL-01 | — |

### 模块: 故障管理 (FAULT)

| ID | 任务 | 状态 | Agent | 依赖 |
|----|------|------|-------|------|
| FAULT-01 | 故障码解析与显示 | 🔵 Todo | — | PROTO-01 |
| FAULT-02 | 故障报警（颜色/声音/弹窗） | 🔵 Todo | FAULT-01 | — |
| FAULT-03 | 调试模式 vs 测试模式 故障响应策略 | 🔵 Todo | FAULT-02 | — |
| FAULT-04 | 故障复位命令 | 🔵 Todo | PROTO-04 | — |

### 模块: 数据保存与导出 (EXPORT)

| ID | 任务 | 状态 | Agent | 依赖 |
|----|------|------|-------|------|
| EXPO-01 | CSV导出（含表头+数据） | 🔵 Todo | — | DATA-01 |
| EXPO-02 | 导出时附带可视化HTML | 🔵 Todo | EXPO-01 | — |

### 模块: 工程文件 (PROJECT)

| ID | 任务 | 状态 | Agent | 依赖 |
|----|------|------|-------|------|
| PROJ-01 | 工程文件 JSON 结构定义 | 🔵 Todo | — | — |
| PROJ-02 | 工程文件保存（串口配置+页面布局+参数） | 🔵 Todo | PROJ-01 | — |
| PROJ-03 | 工程文件导入（设置中加载） | 🔵 Todo | PROJ-01 | — |

### 模块: 主框架 (SHELL)

| ID | 任务 | 状态 | Agent | 依赖 |
|----|------|------|-------|------|
| SHELL-01 | 主窗口框架（菜单栏+工具栏+状态栏） | 🔵 Todo | — | — |
| SHELL-02 | 多页面容器（TabWidget） | 🔵 Todo | — | — |
| SHELL-03 | 串口连接状态指示 | 🔵 Todo | SERIAL-02 | — |
| SHELL-04 | 设置对话框（串口配置+工程文件导入） | 🔵 Todo | — | — |

---

## v0.5: 自动化测试（待规划）

| ID | 任务 | 状态 | 依赖 |
|----|------|------|------|
| AUTO-01 | 测试流程表格编辑器 | 🟣 Backlog | v0.1 |
| AUTO-02 | 测试执行引擎 | 🟣 Backlog | v0.1 |
| AUTO-03 | 断言条件判断 | 🟣 Backlog | v0.1 |
| AUTO-04 | 测试报告生成（PDF） | 🟣 Backlog | v0.1 |
| AUTO-05 | 日志系统（测试时自动保存） | 🟣 Backlog | v0.1 |
| AUTO-06 | 测试序列编排（多测试串联） | 🟣 Backlog | v0.1 |

## v1.0: 完整功能（待规划）

| ID | 任务 | 状态 | 依赖 |
|----|------|------|------|
| V10-01 | 多设备同时连接 | 🟣 Backlog | v0.5 |
| V10-02 | 历史数据回放 | 🟣 Backlog | v0.5 |
| V10-03 | 电机参数辨识辅助 | 🟣 Backlog | v0.5 |
| V10-04 | 串口固件升级 | 🟣 Backlog | v0.5 |
| V10-05 | 稳定性与性能优化 | 🟣 Backlog | v0.5 |

---

## 里程碑

| 里程碑 | 目标日期 | 条件 |
|--------|---------|------|
| M1: v0.1 完成 | — | 所有 SERIAL/PROTO/DATA/CURVE/DASH/PARAM/CTRL/FAULT/EXPO/PROJ/SHELL 任务 Done |
| M2: v0.5 完成 | — | 所有 AUTO 任务 Done |
| M3: v1.0 完成 | — | 所有 V10 任务 Done |