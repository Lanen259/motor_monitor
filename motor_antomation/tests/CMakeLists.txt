enable_testing()

# 测试可执行文件
add_executable(MotorStudioTests
    test_main.cpp
)

target_link_libraries(MotorStudioTests PRIVATE
    Qt6::Core
    Qt6::Test
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

# 注册测试
add_test(NAME MotorStudioTests COMMAND MotorStudioTests)

# Phase 3: 实时数据链路测试
add_executable(MotorStudioPhase3Test
    test_phase3.cpp
)

target_link_libraries(MotorStudioPhase3Test PRIVATE
    Qt6::Core
    MotorStudioCore
    MotorStudioCommunication
    MotorStudioDataBus
    MotorStudioData
    MotorStudioCurve
    MotorStudioDevice
)

add_test(NAME MotorStudioPhase3Test COMMAND MotorStudioPhase3Test)