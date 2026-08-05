add_library(MotorStudioDataBus STATIC
    DataBus.h
    DataBus.cpp
    RingBuffer.h
    Topic.h
)

target_include_directories(MotorStudioDataBus PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries(MotorStudioDataBus PUBLIC Qt5::Core MotorStudioCore)