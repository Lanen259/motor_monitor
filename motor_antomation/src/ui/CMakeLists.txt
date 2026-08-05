add_library(MotorStudioUi STATIC
    # 暂时为空，后续添加 UI 文件
)

target_include_directories(MotorStudioUi PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries(MotorStudioUi PUBLIC
    Qt6::Core
    Qt6::Widgets
    Qt6::Qml
    MotorStudioCore
    MotorStudioDataBus
    MotorStudioCurve
    MotorStudioParameter
    MotorStudioAutomation
)