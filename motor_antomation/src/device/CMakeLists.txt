add_library(MotorStudioDevice STATIC
    DeviceSimulator.h
    DeviceSimulator.cpp
)

target_include_directories(MotorStudioDevice PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries(MotorStudioDevice PUBLIC
    Qt6::Core
    MotorStudioCore
    MotorStudioCommunication
)