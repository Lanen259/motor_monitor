status: done
files_changed: [tests/integration/test_pipeline_integration.cpp, tests/integration/test_mock_mcu_curve.cpp, tests/CMakeLists.txt]
evidence:
  - what_was_done: |
      1. test_pipeline_integration.cpp: DeviceSimulator(500Hz/12ch) → JustFloat serialization → VofaParser → DataBus::publishFrame → CurveEngine subscription. Run 2500ms, verify ≥100 data points per channel.
      2. test_mock_mcu_curve.cpp: Same pipeline, run 5000ms, verify totalWritten()>0, frame loss <5%, data range non-constant for Ia/Ib/Ic/Speed.
      3. Updated CMakeLists.txt: add_executable + add_test for both, linking MotorStudioDevice/Communication/DataBus/Curve/Core libraries.
risks:
  - No top-level CMakeLists.txt (qmake is primary build) — cmake/ctest path incomplete until CMake infrastructure is fully set up
  - VofaParser.cpp compiled directly (not in a library target) — duplicates symbol if linked twice
decisions:
  - problem: DeviceSimulator produces MotorProtocol binary frames but VofaParser expects JustFloat/FireWater | chosen: Serialize MotorDataPayload fields directly to JustFloat bytes → feed to VofaParser | reason: Avoids MotorProtocol encode/decode roundtrip; tests VofaParser→DataBus→CurveEngine accurately
  - problem: ITransport needed by DeviceSimulator | chosen: Use only dataGenerated signal → serialize → VofaParser::feed | reason: Isolates pipeline from transport layer; transport tests belong in test_phase3.cpp
