status: done
files_changed: [DashboardWidget.h, DashboardWidget.cpp]
evidence:
  - what_was_done: |
      Replaced DashboardCell with DashboardCard (QFrame subclass): title bar, large value,
      unit, severity-based background (Normal/Warning/Critical with left accent border),
      adaptive font scaling via resizeEvent. CardType enum (MotorState/CommState/Voltage/
      Current/Speed/Temperature/FaultCount/Generic) with auto-detection from topic name.
      State cards display semantic text (STOPPED/RUNNING/FAULT). Three-level thresholds
      with defaults per card type. Responsive grid (2 cols min, ~220px/card).
      Backward compatible: all existing public API preserved.
risks:
  - Default thresholds may need tuning for actual motor specs
  - Card type detection heuristics may misclassify generic channel names
decisions:
  - problem: Card type detection from auto-generated topic names
    chosen: Substring heuristic matching keywords (motor/comm/volt/curr/speed/temp/fault)
    reason: Simple; works correctly when channels renamed meaningfully via ChannelConfigDialog
  - problem: Font scaling approach
    chosen: scaleFonts() in resizeEvent, proportional to card height with qBound constraints
    reason: Natural DPI scaling; qBound prevents unreadable/oversized fonts
