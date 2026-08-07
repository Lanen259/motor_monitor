status: done
files_changed: [Topic.h, Topic.cpp, ChannelConfigDialog.h, ChannelConfigDialog.cpp, CurveWidget.cpp, DashboardWidget.cpp, test_topic_registry.cpp]
evidence:
  - what_was_done: |
      AC1: CurveWidget::onPullTimer() syncs channel name+color from TopicRegistry every frame.
      DashboardWidget::onRefreshTimer() syncs name+unit from TopicRegistry per cycle.
      AC2: ChannelConfigDialog with 6 columns (Name/Unit/Type/Scale/Offset/Color), QColorDialog
      for color picking, applyToRegistry() persists all descriptor fields.
      AC3: CurveWidget reads ChannelDescriptor::color from TopicRegistry every frame →
      drawLegend/drawCurves use it → color changes propagate automatically.
      AC4: registerTopic(ChannelDescriptor) uses topicId-first lookup → renames handled
      without duplicating entries.
risks:
  - Device reconnect may create new IDs if channel count changes; renamed channels preserved but mapped differently
decisions:
  - problem: channel rename created duplicate entries
    chosen: topicId-priority lookup before name-based lookup
    reason: ChannelConfigDialog flow uses topicId from existing descriptor; this handles renames cleanly
  - problem: CurveWidget only synced names on creation
    chosen: per-frame sync loop updating name and color from TopicRegistry
    reason: single source of truth; changes take effect next frame without widget rebuild
