#pragma once
#include <cstdint>
#include <vector>
#include <string>
#include <functional>

namespace MotorStudio {

// CRC16 校验结果
struct FrameCheckResult {
    bool valid = false;
    std::vector<uint8_t> payload;
};

// 协议层抽象接口
class IProtocol {
public:
    virtual ~IProtocol() = default;

    // 编码：命令 -> 帧
    virtual std::vector<uint8_t> encode(uint16_t cmdId, const std::vector<uint8_t>& payload) = 0;

    // 解码：原始字节 -> 帧校验
    virtual void feed(const std::vector<uint8_t>& rawData) = 0;

    // 协议名称
    virtual std::string protocolName() const = 0;

    // 解码回调
    std::function<void(const FrameCheckResult&)> onFrameDecoded;
    std::function<void(const std::string&)> onError;
};

} // namespace MotorStudio