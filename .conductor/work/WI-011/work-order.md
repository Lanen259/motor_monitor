WI-ID: WI-011
类型: feature
负责人: worker
依赖: [WI-006 ✅]
范围:
  修改: [DashboardWidget.h, DashboardWidget.cpp]
验收标准:
  - 卡片布局 (QGridLayout)，每卡片独立指标
  - 7种卡片类型：MotorState/CommState/Voltage/Current/Speed/Temperature/FaultCount
  - 卡片含标题栏+大字号数值+单位
  - 三级阈值告警 (Normal/Warning/Critical) + 背景变色
  - 自适应尺寸缩放 (字体+列数)
  - 保持现有 public API 向后兼容
非目标: [不涉及动态控件]
状态: done
