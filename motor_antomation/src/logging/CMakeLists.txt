add_library(MotorStudioLogging STATIC
    Logger.h
    Logger.cpp
)

target_include_directories(MotorStudioLogging PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries(MotorStudioLogging PUBLIC
    Qt5::Core
    MotorStudioCore
)