add_library(MotorStudioApp STATIC
    Application.h
    Application.cpp
)

target_include_directories(MotorStudioApp PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries(MotorStudioApp PUBLIC
    Qt6::Core
    MotorStudioCore
    MotorStudioCommunication
    MotorStudioDataBus
    MotorStudioData
    MotorStudioCurve
    MotorStudioParameter
    MotorStudioAutomation
    MotorStudioPlugin
    MotorStudioLogging
    MotorStudioScripting
)