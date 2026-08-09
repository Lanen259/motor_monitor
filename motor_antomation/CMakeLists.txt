# Motor Studio — 顶层 CMake 构建（测试门禁用）
# 主构建仍是 qmake（见 motor_antomation.pro）。本文件把分散的模块 CMakeLists
# 串成完整构建树，使单元 + 集成测试能在 CTest 下真实构建、运行，
# 作为本地与 CI 共用的"合并门禁"（CLAUDE.md §4 的机器化）。
cmake_minimum_required(VERSION 3.16)
project(MotorStudio LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 与 qmake 的 INCLUDEPATH += src 对齐，避免模块间头文件包含传递问题
include_directories(${CMAKE_SOURCE_DIR}/src)

# Qt 元对象编译：全树启用 AUTOMOC（多数模块头文件含 Q_OBJECT，
# 部分 cpp 直接 #include "xxx.moc"；手写 moc 清单不可持续）
set(CMAKE_AUTOMOC ON)

# 严格编译选项（-Wall -Wextra -Wpedantic -Werror）默认关闭：
# 首次跑通门禁后再取消下面一行注释，把 warning 当 error 强制清零。
# include(${CMAKE_CURRENT_LIST_DIR}/cmake/CompilerSettings.cmake)

# Qt5 组件：应用用到的 + 测试用到的
find_package(Qt5 REQUIRED COMPONENTS
    Core Widgets Qml SerialPort SerialBus Network Test
)

# 必须在顶层启用 CTest，否则从构建根目录跑 ctest 找不到子目录注册的测试
enable_testing()

# 所有库模块（按依赖顺序，见 src/CMakeLists.txt）+ 测试树
add_subdirectory(src)
add_subdirectory(tests)
