add_library(MotorStudioParameter STATIC
    ParameterManager.h
    ParameterManager.cpp
    ParameterTypes.h
)

target_include_directories(MotorStudioParameter PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries(MotorStudioParameter PUBLIC
    Qt6::Core
    MotorStudioCore
    MotorStudioDataBus
)