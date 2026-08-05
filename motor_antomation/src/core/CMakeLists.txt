add_library(MotorStudioCore STATIC
    event/EventBus.h
    event/EventBus.cpp
    message/Message.h
    manager/ModuleManager.h
    manager/ModuleManager.cpp
    manager/SettingsManager.h
    manager/SettingsManager.cpp
)

target_include_directories(MotorStudioCore PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries(MotorStudioCore PUBLIC Qt5::Core)