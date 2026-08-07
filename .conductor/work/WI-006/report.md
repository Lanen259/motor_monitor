status: done
files_changed: [motor_antomation/mainwindow.h, motor_antomation/mainwindow.cpp]
evidence:
  - what_was_done: |
      Transformed MainWindow from QTabWidget to industrial 4-zone layout:
      1. Left nav (QDockWidget + QListWidget): 5 entries (Dashboard/Oscilloscope/Automation/Device/Settings)
      2. Central workspace (QStackedWidget): all existing widgets preserved at indices 0-4
      3. Right properties (QDockWidget + QStackedWidget): per-page dynamic property panels
      4. Bottom log console (QDockWidget): QPlainTextEdit + Clear/Auto-scroll toggle
      Navigation switches both workspace AND property panels via currentRowChanged.
      Added static MainWindow::instance() and MainWindow::log() for global logging.
      Added AutomationWidget/AutomationEngine/TestRunner forward refs for P3 integration.
risks:
  - Qt/qmake not available; compilation not verified in this environment
decisions:
  - problem: Some nav pages have no properties yet
    chosen: Per-page property panels with dynamic info (dashboard: channels+rate; oscilloscope: curves+FPS; settings: param count; others: placeholder)
    reason: Exceeds minimum bar; QStackedWidget scales naturally for future work items
