WI-ID: WI-006
类型: feature
负责人: worker
依赖: []
范围:
  修改: [motor_antomation/mainwindow.h, motor_antomation/mainwindow.cpp]
  新增: []
验收标准:
  - MainWindow 布局改为：左侧导航栏（QListWidget）+ 中央 QStackedWidget 工作区 + 右侧属性面板（QDockWidget）+ 底部日志控制台（QDockWidget）
  - 左侧导航包含条目：Dashboard, Oscilloscope, Automation, Device, Settings（至少5项）
  - 点击导航项切换中央工作区页面（QStackedWidget setCurrentIndex）
  - 右侧属性面板默认显示当前选中项的属性（至少显示占位文本 "Select an item to view properties"）
  - 底部日志控制台包含 QPlainTextEdit（只读）+ 工具栏（Clear, Auto-scroll 按钮）
  - 现有的 CurveWidget/DashboardWidget/FaultWidget/ParameterWidget 全部保留并嵌入新布局的工作区
非目标: [不涉及卡片化 Dashboard（那是 P2-02）、不涉及动态控件（那是 P2-03）]
状态: done
