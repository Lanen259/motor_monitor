WI-ID: WI-005
类型: feature
负责人: worker
依赖: [P0-02]
范围:
  修改: [Topic.h, Topic.cpp, ChannelConfigDialog.h, ChannelConfigDialog.cpp, CurveWidget.cpp, DashboardWidget.cpp, test_topic_registry.cpp]
验收标准:
  - ChannelRegistry descriptor 更新后，Dashboard/Curve 自动使用新名称/单位/颜色/缩放
  - 用户可通过 ChannelConfigDialog 新增通道，输入名称/单位/类型/缩放/偏移/颜色，点击 Apply 后 TopicRegistry 已更新
  - 通道颜色通过 ChannelConfigDialog 设置后正确反映在 CurveWidget 图例和曲线颜色上
  - TopicRegistry::registerTopic(ChannelDescriptor) 支持增量更新（同名通道更新 descriptor，不新增 ID）
非目标: [不涉及多窗口曲线、不涉及曲线管理面板（那是 P1-03）]
状态: done
