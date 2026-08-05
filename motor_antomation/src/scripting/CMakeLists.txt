add_library(MotorStudioScripting STATIC
    IScriptEngine.h
    LuaEngine.h
    LuaEngine.cpp
)

target_include_directories(MotorStudioScripting PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries(MotorStudioScripting PUBLIC
    Qt6::Core
    MotorStudioCore
    MotorStudioDataBus
    MotorStudioAutomation
)