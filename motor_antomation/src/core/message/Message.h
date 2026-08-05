#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <chrono>

namespace MotorStudio {

// 命令消息
struct Command {
    uint16_t cmdId;
    std::vector<uint8_t> payload;
    uint32_t timeoutMs = 1000;
    int priority = 0;  // 0=normal, 1=high, 2=critical
};

// 响应消息
struct Response {
    uint16_t cmdId;
    uint16_t status;
    std::vector<uint8_t> data;
    std::chrono::steady_clock::time_point timestamp;
};

// 实时数据点
struct DataPoint {
    uint32_t topicId;
    float value;
    uint64_t timestampUs;
};

} // namespace MotorStudio