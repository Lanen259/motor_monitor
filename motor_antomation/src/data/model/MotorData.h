#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <chrono>

namespace MotorStudio {

// 电机参数
struct MotorParams {
    std::string name;
    uint8_t polePairs = 4;
    float ratedVoltage = 24.0f;
    float ratedCurrent = 10.0f;
    float ratedSpeed = 3000.0f;
    float resistance = 0.1f;
    float inductance = 0.0001f;
};

// 电机运行状态
enum class MotorState : uint8_t {
    Idle,
    Running,
    Fault,
    Calibrating,
    Braking
};

// 实时数据快照
struct MotorSnapshot {
    uint64_t timestampUs = 0;
    float ia = 0, ib = 0, ic = 0;
    float id = 0, iq = 0;
    float speed = 0;
    float position = 0;
    float busVoltage = 0;
    float busCurrent = 0;
    float temperature = 0;
    MotorState state = MotorState::Idle;
    uint32_t faultCode = 0;
};

} // namespace MotorStudio