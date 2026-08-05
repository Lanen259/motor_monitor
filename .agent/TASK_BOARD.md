# TASK_BOARD.md — Motor Automation 任务看板

> 最后更新: 2026-08-05T21:00+08:00
> 当前版本: v0.1 ✅ 完成
> 构建系统: qmake (Qt 5.14.2 MinGW 7.3.0 32-bit)

---

## 看板说明

- 🟢 Done — 已完成（编译通过，功能可用）

---

## v0.1: 基础调试 ✅ 全部完成

### SHELL — 主框架 🟢
| ID | 任务 | 状态 |
|----|------|------|
| SHELL-01 | MainWindow 主框架（菜单栏+工具栏+状态栏） | 🟢 |
| SHELL-02 | 多页面容器（QTabWidget） | 🟢 |
| SHELL-03 | 串口选择/波特率/连接按钮 | 🟢 |
| SHELL-04 | 窗口状态持久化 + 关于对话框 | 🟢 |

### SERIAL — 串口通讯 🟢
| ID | 任务 | 状态 |
|----|------|------|
| SERIAL-01 | 串口扫描与枚举 | 🟢 |
| SERIAL-02 | SerialTransport 连接/断开管理 | 🟢 |
| SERIAL-03 | 波特率/数据位/停止位配置 | 🟢 |
| SERIAL-04 | 断线检测与自动关闭 | 🟢 |

### PROTOCOL — VOFA+协议解析 🟢
| ID | 任务 | 状态 |
|----|------|------|
| PROTOCOL-01 | JustFloat 帧解析器 | 🟢 |
| PROTOCOL-02 | FireWater 分隔符帧解析 | 🟢 |
| PROTOCOL-03 | 多通道数据提取 | 🟢 |
| PROTOCOL-04 | 协议错误处理 | 🟢 |

### DATABUS — 实时数据总线 🟢
| ID | 任务 | 状态 |
|----|------|------|
| DATABUS-01 | ChannelRingBuffer 15分钟环形缓冲 | 🟢 |
| DATABUS-02 | ChannelManager 多通道管理 | 🟢 |
| DATABUS-03 | DataBus 与 VofaParser 集成 | 🟢 |

### CURVE — 曲线波形 🟢
| ID | 任务 | 状态 |
|----|------|------|
| CURVE-01 | CurveWidget 多通道实时曲线 | 🟢 |
| CURVE-02 | 降采样显示（2000点/通道） | 🟢 |
| CURVE-03 | 自动缩放 + 手动缩放 + 平移 | 🟢 |
| CURVE-04 | 通道颜色 + 图例 | 🟢 |

### DASHBOARD — 仪表盘 🟢
| ID | 任务 | 状态 |
|----|------|------|
| DASHBOARD-01 | DashboardCell 数值显示 | 🟢 |
| DASHBOARD-02 | DashboardWidget 网格布局 | 🟢 |
| DASHBOARD-03 | 警告阈值变色 | 🟢 |

### PARAM — 参数系统 🟢
| ID | 任务 | 状态 |
|----|------|------|
| PARAM-01 | ParameterManager 参数读写 | 🟢 |
| PARAM-02 | JSON 导入导出 | 🟢 |
| PARAM-03 | ParameterWidget 参数编辑面板 | 🟢 |
| PARAM-04 | 文件持久化 | 🟢 |

### FAULT — 故障管理 🟢
| ID | 任务 | 状态 |
|----|------|------|
| FAULT-01 | FaultWidget 故障列表显示 | 🟢 |
| FAULT-02 | 硬件/软件故障区分 | 🟢 |
| FAULT-03 | 测试模式硬件故障报警 | 🟢 |
| FAULT-04 | 故障计数与状态指示 | 🟢 |

### EXPORT — CSV导出 🟢
| ID | 任务 | 状态 |
|----|------|------|
| EXPORT-01 | CSV 导出（时间戳+所有通道） | 🟢 |
| EXPORT-02 | 曲线截图保存 | 🟢 |

### PROJECT — 工程文件 🟢
| ID | 任务 | 状态 |
|----|------|------|
| PROJECT-01 | JSON 工程文件保存 | 🟢 |
| PROJECT-02 | JSON 工程文件加载 | 🟢 |
| PROJECT-03 | 参数随工程持久化 | 🟢 |

---

## 数据管道

```
SerialTransport → VofaParser → ChannelManager → CurveWidget
                                               → DashboardWidget
                                               → CSV Export
ParameterManager ↔ ParameterWidget ↔ JSON File
FaultWidget ← 故障码解析
```

## Git 提交历史

```
4facb1e [cleanup] 移除残留文件
1604838 [PARAM][CONTROL][FAULT][EXPORT][PROJECT] v0.1 全部模块
0da9932 [CURVE][DASHBOARD] 曲线波形 + 仪表盘
25a929b [DATABUS] 实时数据总线
f320f3a [PROTOCOL] VOFA+ 协议解析
4b5cb80 [SERIAL] 串口通讯
4718d63 [SHELL] 主框架
```