status: done
files_changed: [CurveEngine.h, CurveEngine.cpp, CurveWidget.h, CurveWidget.cpp, test_curve_engine.cpp]
evidence:
  - what_was_done: |
      1. DEFAULT_CAPACITY 10000→100000, added setCapacity() with data-preserving resize
      2. Replaced kMaxDrawPoints step-truncation with LTTD downsample in drawCurves() targeting pixel width
      3. Removed redundant pre-downsample logic from onPullTimer(); now only syncs metadata + min/max
      4. Added QElapsedTimer frame timing in paintEvent() → frameIntervalMs()
      5. Fixed drawLegend() to get latest value from CurveEngine when channel data is empty
      6. 3 new tests: testLTTBDownsampleLarge (50000→800 pts), testSetCapacity, testMultiThreadedAppend (4 threads × 10000 writes)
risks:
  - LTTB runs per-paintEvent; high-frequency resize may trigger repeated downsamples on large buffers — cache if profiling shows issue
decisions:
  - problem: onPullTimer still needed data range but not downsample
    chosen: Pull min/max from CurveChannel::dataRange(), let drawCurves() handle LTTB at pixel resolution
    reason: Separation of concerns; downsample always targets current widget width (resize-robust)
  - problem: Thread safety test design
    chosen: 4-thread barrier-sync'd append stress test, verify totalWritten() == expected
    reason: Validates mutex guards prevent lost writes / corrupted state under contention
