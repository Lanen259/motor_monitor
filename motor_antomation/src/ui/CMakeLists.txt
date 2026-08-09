# UI 模块：qmake 递归 glob 收全部源文件（CLAUDE.md §1），
# CMake 侧同样 glob，避免新增 widget 后漏登记导致 CMake 测试构建缺符号。
file(GLOB UI_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp ${CMAKE_CURRENT_SOURCE_DIR}/*.h)

add_library(MotorStudioUi STATIC ${UI_SOURCES})

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
