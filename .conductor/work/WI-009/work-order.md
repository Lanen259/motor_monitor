WI-ID: WI-009
类型: feature
负责人: worker
依赖: [WI-005 ✅, WI-008 ✅]
范围:
  修改: [CurveWidget.h/cpp, Topic.h/cpp, mainwindow.h/cpp]
  新增: [CurveManagerPanel.h/cpp]
验收标准:
  - QTableWidget (7列: 名称/颜色/Y-min/Y-max/可见/操作)
  - 双击颜色 → QColorDialog → 实时反映到所有 CurveWidget
  - 可见复选框 → 曲线显示/隐藏
  - 删除按钮 → 从所有CurveWidget+CurveEngine+TopicRegistry删除
  - +添加 → ChannelConfigDialog → 注册到 CurveEngine
  - 多窗口选择 QComboBox
  - 嵌入 Oscilloscope 页面 (QSplitter 上75%曲线 下25%管理)
非目标: [不涉及动态通道注册]
状态: done
