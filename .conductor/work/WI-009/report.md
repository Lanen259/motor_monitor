status: done
files_changed: [CurveManagerPanel.h, CurveManagerPanel.cpp, Topic.h, Topic.cpp, CurveWidget.h, CurveWidget.cpp, mainwindow.h, mainwindow.cpp]
evidence:
  - what_was_done: |
      1. CurveManagerPanel: QTableWidget 7-column (Name/Color/Y-min/Y-max/Visible/Actions) with inline editing
      2. Color: double-click → QColorDialog → TopicRegistry persistence → applyColorToAllCurves()
      3. Visibility: checkbox per row with topicId property → applyVisibilityToAllCurves()
      4. Delete: confirmation dialog → remove from all CurveWidgets → CurveEngine → TopicRegistry::removeTopic()
      5. Add: opens ChannelConfigDialog → registers to CurveEngine → refresh panel
      6. Multi-window: QComboBox target selector
      7. Integration: Oscilloscope page QSplitter (MultiCurveContainer 75% / CurveManagerPanel 25%)
      8. TopicRegistry::removeTopic(TopicId) added with mutex protection
      9. CurveWidget accessors: channelName/setChannelVisible/isChannelVisible/channelTopicId
risks:
  - Y-min/Y-max per-row but applies to widget-global Y axis — last edit wins
  - Deleted channel may be re-created by auto-registration on next frame
decisions:
  - problem: Checkbox/button signals captured row index (stale on refresh)
    chosen: Store topicId as widget property, lookup by topicId in slot
    reason: Row-independence across table rebuilds
  - problem: Y-min/Y-max per-channel vs per-widget
    chosen: Per-row stored, applied to widget-level Y axis (auto-scale disabled)
    reason: CurveWidget uses single Y-axis; most practical without refactoring draw architecture
