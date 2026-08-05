add_library(MotorStudioCurve STATIC
    CurveEngine.h
    CurveEngine.cpp
    CurveRenderer.h
    CurveRenderer.cpp
)

target_include_directories(MotorStudioCurve PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries(MotorStudioCurve PUBLIC
    Qt6::Core
    Qt6::Widgets
    MotorStudioCore
    MotorStudioDataBus
    MotorStudioData
)