add_library(MotorStudioPlugin STATIC
    IPlugin.h
    PluginLoader.h
    PluginLoader.cpp
)

target_include_directories(MotorStudioPlugin PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries(MotorStudioPlugin PUBLIC
    Qt5::Core
    MotorStudioCore
)