add_library(MotorStudioCommunication STATIC
    transport/ITransport.h
    transport/SerialTransport.h
    transport/SerialTransport.cpp
    transport/CanTransport.h
    transport/CanTransport.cpp
    transport/TcpTransport.h
    transport/TcpTransport.cpp
    transport/UdpTransport.h
    transport/UdpTransport.cpp
    transport/LoopbackTransport.h
    transport/LoopbackTransport.cpp
    protocol/IProtocol.h
    protocol/FrameCodec.h
    protocol/FrameCodec.cpp
    protocol/MotorProtocol.h
    protocol/MotorProtocol.cpp
    protocol/CommandQueue.h
    protocol/CommandQueue.cpp
    DeviceManager.h
    DeviceManager.cpp
)

target_include_directories(MotorStudioCommunication PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries(MotorStudioCommunication PUBLIC
    Qt5::Core
    Qt5::SerialPort
    Qt5::SerialBus
    Qt5::Network
    MotorStudioCore
)