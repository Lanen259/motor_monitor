status: done
files_changed: [DynamicWidgetFactory.h, DynamicWidgetFactory.cpp, WidgetBindingManager.h, WidgetBindingManager.cpp, mainwindow.h, mainwindow.cpp]
evidence:
  - what_was_done: |
      1. DynamicWidgetFactory creates styled QPushButton/QSlider/QLineEdit with industrial dark theme
      2. WidgetBindingManager stores widget→command in QMap, connects clicked/valueChanged/returnPressed
         to executeCommand() with {value} placeholder substitution
      3. Toolbar "+" button opens modal creation dialog (type combo + label + command + OK/Cancel)
      4. Dynamic widgets dock on right side with vertical layout
      5. Right-click context menu: Edit Binding (QInputDialog pre-filled) + Delete (unbind + remove)
      6. commandExecuted signal → MainWindow catches → logs via MainWindow::log()
risks:
  - No DataBus command topic exists; commands routed through logging (extensible via signal)
decisions:
  - problem: Where to place dynamic widgets
    chosen: Dedicated QDockWidget on right side below property dock
    reason: Consistent with dock-based layout; movable/floating
  - problem: No DataBus topic for string commands
    chosen: Signal-based commandExecuted → MainWindow::log()
    reason: DataBus publishes float DataPoints; signal approach is extensible
