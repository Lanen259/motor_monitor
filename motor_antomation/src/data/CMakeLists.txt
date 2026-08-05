add_library(MotorStudioData STATIC
    model/MotorData.h
    buffer/DataBuffer.h
    buffer/DataBuffer.cpp
    manager/DataManager.h
    manager/DataManager.cpp
    recorder/DataRecorder.h
    recorder/DataRecorder.cpp
)

target_include_directories(MotorStudioData PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries(MotorStudioData PUBLIC
    Qt6::Core
    MotorStudioCore
    MotorStudioDataBus
)