add_library(MotorStudioAutomation STATIC
    AutomationEngine.h
    AutomationEngine.cpp
    TestRunner.h
    TestRunner.cpp
    VariableScope.h
    VariableScope.cpp
    FlowGraph.h
    FlowGraph.cpp
    ExpressionEngine.h
    ExpressionEngine.cpp
    FlowRunner.h
    FlowRunner.cpp
)

target_include_directories(MotorStudioAutomation PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries(MotorStudioAutomation PUBLIC
    Qt5::Core
    MotorStudioCore
    MotorStudioDataBus
    MotorStudioParameter
    MotorStudioCommunication
)