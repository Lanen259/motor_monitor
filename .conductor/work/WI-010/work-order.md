WI-ID: WI-010
类型: feature
负责人: worker
依赖: [P0-03 ✅, WI-008 ✅]
范围:
  修改: [CurveEngine.h/cpp, CurveWidget.h/cpp, test_curve_engine.cpp]
验收标准:
  - LTTB 降采样接入 drawCurves() 渲染路径，替换 kMaxDrawPoints 截断
  - 默认容量 100000 点/通道，setCapacity() 可配置
  - paintEvent 帧计时 (QElapsedTimer)
  - 双重 mutex 线程安全 (CurveEngine + CurveChannel)
  - 3 个新单测：LTTB 降采样/容量配置/多线程并发 append
非目标: [不涉及 UI 布局、不涉及曲线管理器]
状态: done
