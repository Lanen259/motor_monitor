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
