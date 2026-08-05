add_library(MotorStudioUi STATIC
    # 暂时为空，后续添�?UI 文件
)

target_include_directories(MotorStudioUi PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries(MotorStudioUi PUBLIC
    Qt5::Core
    Qt5::Widgets
    Qt5::Qml
    MotorStudioCore
    MotorStudioDataBus
    MotorStudioCurve
    MotorStudioParameter
    MotorStudioAutomation
)