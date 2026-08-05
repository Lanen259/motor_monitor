#pragma once
#include "IProtocol.h"
#include <memory>

namespace MotorStudio {

// 通用帧编解码器（COBS + CRC16）
class FrameCodec : public IProtocol {
public:
    FrameCodec();
    ~FrameCodec() override;

    std::vector<uint8_t> encode(uint16_t cmdId, const std::vector<uint8_t>& payload) override;
    void feed(const std::vector<uint8_t>& rawData) override;
    std::string protocolName() const override { return "FrameCodec"; }

private:
    struct Impl;
    std::unique_ptr<Impl> d;
};

} // namespace MotorStudio