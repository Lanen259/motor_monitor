WI-ID: WI-008
类型: feature
负责人: worker
依赖: [WI-005 ✅]
范围:
  修改: [CurveWidget.h/cpp, mainwindow.h/cpp]
  新增: [MultiCurveContainer.h/cpp]
验收标准:
  - Tab 模式：QTabWidget 可新增/关闭/重命名 Tab
  - Grid 模式：2x2/3x2/4x3 网格布局，时间轴同步
  - Tab/Grid 模式可切换，状态保持
  - 每个 CurveWidget 独立通道列表/Y轴/颜色
非目标: [不涉及曲线管理面板（WI-009）、不涉及 LTTB（WI-010）]
状态: done
