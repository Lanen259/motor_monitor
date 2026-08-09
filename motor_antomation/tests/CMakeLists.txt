enable_testing()

# Existing: placeholder test
add_executable(MotorStudioTests
    test_main.cpp
)

target_link_libraries(MotorStudioTests PRIVATE
    Qt5::Core
    Qt5::Test
    MotorStudioCore
    MotorStudioCommunication
    MotorStudioDataBus
    MotorStudioData
    MotorStudioCurve
    MotorStudioDevice
    MotorStudioParameter
    MotorStudioAutomation
    MotorStudioLogging
    MotorStudioPlugin
)

add_test(NAME MotorStudioTests COMMAND MotorStudioTests)

# Phase 3: Real-time data link benchmark (existing)
add_executable(MotorStudioPhase3Test
    test_phase3.cpp
)

target_link_libraries(MotorStudioPhase3Test PRIVATE
    Qt5::Core
    MotorStudioCore
    MotorStudioCommunication
    MotorStudioDataBus
    MotorStudioData
    MotorStudioCurve
    MotorStudioDevice
)

add_test(NAME MotorStudioPhase3Test COMMAND MotorStudioPhase3Test)

# === P0-04: Unit Tests ===

# TopicRegistry unit test
add_executable(TestTopicRegistry
    unit/databus/test_topic_registry.cpp
    ${CMAKE_SOURCE_DIR}/src/databus/Topic.cpp
)

target_include_directories(TestTopicRegistry PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(TestTopicRegistry PRIVATE
    Qt5::Core
    Qt5::Test
)

add_test(NAME TestTopicRegistry COMMAND TestTopicRegistry)

# DataBus unit test
add_executable(TestDataBus
    unit/databus/test_databus.cpp
    ${CMAKE_SOURCE_DIR}/src/databus/Topic.cpp
    ${CMAKE_SOURCE_DIR}/src/databus/DataBus.cpp
)

target_include_directories(TestDataBus PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(TestDataBus PRIVATE
    Qt5::Core
    Qt5::Test
)

add_test(NAME TestDataBus COMMAND TestDataBus)

# CurveEngine unit test
add_executable(TestCurveEngine
    unit/curve/test_curve_engine.cpp
    ${CMAKE_SOURCE_DIR}/src/curve/CurveEngine.cpp
    ${CMAKE_SOURCE_DIR}/src/databus/Topic.cpp
)

target_include_directories(TestCurveEngine PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(TestCurveEngine PRIVATE
    Qt5::Core
    Qt5::Test
)

add_test(NAME TestCurveEngine COMMAND TestCurveEngine)

# VofaParser unit test
add_executable(TestVofaParser
    unit/communication/test_vofa_parser.cpp
    ${CMAKE_SOURCE_DIR}/src/communication/protocol/VofaParser.cpp
)

target_include_directories(TestVofaParser PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(TestVofaParser PRIVATE
    Qt5::Core
    Qt5::Test
)

add_test(NAME TestVofaParser COMMAND TestVofaParser)

# AutomationEngine unit test (WI-007)
add_executable(TestAutomationEngine
    unit/automation/test_automation_engine.cpp
    ${CMAKE_SOURCE_DIR}/src/automation/AutomationEngine.cpp
    ${CMAKE_SOURCE_DIR}/src/automation/TestRunner.cpp
    ${CMAKE_SOURCE_DIR}/src/automation/ReportGenerator.cpp
    ${CMAKE_SOURCE_DIR}/src/databus/Topic.cpp
    ${CMAKE_SOURCE_DIR}/src/databus/DataBus.cpp
)

target_include_directories(TestAutomationEngine PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(TestAutomationEngine PRIVATE
    Qt5::Core
    Qt5::Test
)

add_test(NAME TestAutomationEngine COMMAND TestAutomationEngine)

# 测试用例 JSON 夹具位于源码树 tests/unit/automation/（TEST_DATA_DIR 期望
# "<dataDir>/automation/*.json"，故指向 tests/unit）
set_tests_properties(TestAutomationEngine PROPERTIES
    ENVIRONMENT "TEST_DATA_DIR=${CMAKE_CURRENT_SOURCE_DIR}/unit"
)

# VariableScope unit test (WI-202)
add_executable(TestVariableScope
    unit/automation/test_variable_scope.cpp
    ${CMAKE_SOURCE_DIR}/src/automation/VariableScope.cpp
)

target_include_directories(TestVariableScope PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(TestVariableScope PRIVATE
    Qt5::Core
    Qt5::Test
)

add_test(NAME TestVariableScope COMMAND TestVariableScope)

# FlowGraph unit test (WI-203)
add_executable(TestFlowGraph
    unit/automation/test_flowgraph.cpp
    ${CMAKE_SOURCE_DIR}/src/automation/FlowGraph.cpp
)

target_include_directories(TestFlowGraph PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(TestFlowGraph PRIVATE
    Qt5::Core
    Qt5::Test
)

add_test(NAME TestFlowGraph COMMAND TestFlowGraph)

# ExpressionEngine unit test (WI-201)
# Pure C++ engine, no Qt dependency — includes implementation directly.
add_executable(TestExpressionEngine
    unit/automation/test_expression_engine.cpp
    ${CMAKE_SOURCE_DIR}/src/automation/ExpressionEngine.cpp
)

target_include_directories(TestExpressionEngine PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(TestExpressionEngine PRIVATE
    Qt5::Core
    Qt5::Test
)

add_test(NAME TestExpressionEngine COMMAND TestExpressionEngine)

# === WI-015: Integration Tests ===
# Pipe: DeviceSimulator -> VofaParser -> DataBus -> CurveEngine

# Pipeline Integration Test (WI-015)
# DeviceSimulator@500Hz -> VofaParser(JustFloat) -> DataBus -> CurveEngine
# Verify >=100 data points per channel after 2+ seconds
add_executable(TestPipelineIntegration
    integration/test_pipeline_integration.cpp
    ${CMAKE_SOURCE_DIR}/src/communication/protocol/VofaParser.cpp
)

target_include_directories(TestPipelineIntegration PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(TestPipelineIntegration PRIVATE
    Qt5::Core
    MotorStudioCore
    MotorStudioCommunication
    MotorStudioDataBus
    MotorStudioCurve
    MotorStudioDevice
)

add_test(NAME TestPipelineIntegration COMMAND TestPipelineIntegration)

# Mock MCU Curve Test (WI-015)
# Full pipeline run 5 seconds, verify CurveEngine data non-empty and no data loss
add_executable(TestMockMcuCurve
    integration/test_mock_mcu_curve.cpp
    ${CMAKE_SOURCE_DIR}/src/communication/protocol/VofaParser.cpp
)

target_include_directories(TestMockMcuCurve PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(TestMockMcuCurve PRIVATE
    Qt5::Core
    MotorStudioCore
    MotorStudioCommunication
    MotorStudioDataBus
    MotorStudioCurve
    MotorStudioDevice
)

add_test(NAME TestMockMcuCurve COMMAND TestMockMcuCurve)

# === 夜间防卡死自测：自动化域 UI 交互回归（Phase 2）===
# 看门狗头文件位于 tests/common/（共享规格 §2.2），通过 tests/ 包含目录提供。
# 全部 UI 测试：QT_QPA_PLATFORM=offscreen（环境变量已在 gate 侧设置），
# ctest 单独为交互用例设 TIMEOUT 60 防挂起。

# NodeParamPanel 交互（A-01/A-02 UAF + BUG-001/004 闭环 + A-15）
add_executable(TestNodeParamPanelInteraction
    unit/ui/test_node_param_panel.cpp
    ${CMAKE_SOURCE_DIR}/src/databus/Topic.cpp
    ${CMAKE_SOURCE_DIR}/src/automation/ReportGenerator.cpp
)
target_include_directories(TestNodeParamPanelInteraction PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)
target_link_libraries(TestNodeParamPanelInteraction PRIVATE
    Qt5::Core
    Qt5::Widgets
    Qt5::Test
    MotorStudioUi
)
add_test(NAME TestNodeParamPanelInteraction COMMAND TestNodeParamPanelInteraction)
set_tests_properties(TestNodeParamPanelInteraction PROPERTIES TIMEOUT 60)

# FlowCanvas / AutomationWidget 交互（A-03/A-06/A-07 + SYM-1/2/3）
add_executable(TestFlowCanvasInteraction
    unit/ui/test_flow_canvas.cpp
    ${CMAKE_SOURCE_DIR}/src/databus/Topic.cpp
    ${CMAKE_SOURCE_DIR}/src/automation/ReportGenerator.cpp
)
target_include_directories(TestFlowCanvasInteraction PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)
target_link_libraries(TestFlowCanvasInteraction PRIVATE
    Qt5::Core
    Qt5::Widgets
    Qt5::Test
    MotorStudioUi
)
add_test(NAME TestFlowCanvasInteraction COMMAND TestFlowCanvasInteraction)
set_tests_properties(TestFlowCanvasInteraction PROPERTIES TIMEOUT 60)

# VariableEditorPanel 交互 + 作用域接线（A-14）
add_executable(TestVariableEditorPanelInteraction
    unit/ui/test_variable_editor_panel.cpp
    ${CMAKE_SOURCE_DIR}/src/databus/Topic.cpp
    ${CMAKE_SOURCE_DIR}/src/automation/ReportGenerator.cpp
)
target_include_directories(TestVariableEditorPanelInteraction PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)
target_link_libraries(TestVariableEditorPanelInteraction PRIVATE
    Qt5::Core
    Qt5::Widgets
    Qt5::Test
    MotorStudioUi
)
add_test(NAME TestVariableEditorPanelInteraction COMMAND TestVariableEditorPanelInteraction)
set_tests_properties(TestVariableEditorPanelInteraction PROPERTIES TIMEOUT 60)

# === 夜间防卡死自测：Debug 仿真压力测试（Phase 4）===
# 固定种子随机交互 ≥500 次 + 事件循环看门狗（300ms）。3 个不同种子各一轮。
add_executable(StressAutomationUi
    stress/stress_automation_ui.cpp
    ${CMAKE_SOURCE_DIR}/src/databus/Topic.cpp
    ${CMAKE_SOURCE_DIR}/src/automation/ReportGenerator.cpp
)
target_include_directories(StressAutomationUi PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)
target_link_libraries(StressAutomationUi PRIVATE
    Qt5::Core
    Qt5::Widgets
    Qt5::Test
    MotorStudioUi
)
add_test(NAME StressAutomationUi_seed1 COMMAND StressAutomationUi --seed 1)
add_test(NAME StressAutomationUi_seed2 COMMAND StressAutomationUi --seed 2)
add_test(NAME StressAutomationUi_seed3 COMMAND StressAutomationUi --seed 3)
set_tests_properties(StressAutomationUi_seed1 StressAutomationUi_seed2 StressAutomationUi_seed3
    PROPERTIES TIMEOUT 600)
