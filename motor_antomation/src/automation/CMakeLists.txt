add_library(MotorStudioAutomation STATIC
    AutomationEngine.h
    AutomationEngine.cpp
    TestRunner.h
    TestRunner.cpp
)

target_include_directories(MotorStudioAutomation PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries(MotorStudioAutomation PUBLIC
    Qt6::Core
    MotorStudioCore
    MotorStudioDataBus
    MotorStudioParameter
    MotorStudioCommunication
)