status: done
files_changed: [MultiCurveContainer.h, MultiCurveContainer.cpp, CurveWidget.h, CurveWidget.cpp, mainwindow.h, mainwindow.cpp]
evidence:
  - what_was_done: |
      Created MultiCurveContainer with Tab mode (QTabWidget + add/close/rename) and Grid mode
      (2x2/3x2/4x3 with synchronized time axis). Mode switch via QComboBox preserves all
      CurveWidget instances and data. Extended CurveWidget with timeAxisChanged() signal,
      timeBase()/xRangeSeconds()/setTimeBase() for cross-widget synchronization.
      Integrated into MainWindow replacing single CurveWidget with MultiCurveContainer at
      workspace index 1.
risks:
  - Tab→Grid switching may auto-create empty widgets to fill grid cells
  - Grid→Tab loses previous tab names (resets to "Curve N")
decisions:
  - problem: Grid mode time-axis sync across cells
    chosen: Signal/slot via timeAxisChanged() with blockSignals() to prevent feedback
    reason: Minimal API surface change; feed-forward propagation suffices
  - problem: Widget storage during mode switch
    chosen: Shared QVector<CurveWidget*>; rebuildLayout() detaches/re-attaches
    reason: Preserves all channel data and Y-axis settings; no state loss
