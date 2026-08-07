WI-ID: WI-015
类型: test
负责人: worker
依赖: [P0-04 ✅, P0-02 ✅]
范围:
  修改: [tests/CMakeLists.txt]
  新增: [tests/integration/test_pipeline_integration.cpp, tests/integration/test_mock_mcu_curve.cpp]
验收标准:
  - test_pipeline_integration.cpp: DeviceSimulator(500Hz,12ch) → JustFloat → VofaParser → DataBus → CurveEngine → 验证 ≥100 点/通道
  - test_mock_mcu_curve.cpp: 同样管道运行5s → 验证 totalWritten()>0, 丢包率<5%, 数据范围非恒定
  - 两个测试均挂在 CTest 下
非目标: [不涉及 UI 自动化截图对比]
状态: done
