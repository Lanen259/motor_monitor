#include "FrameCodec.h"

namespace MotorStudio {

struct FrameCodec::Impl {
    // TODO: 帧缓冲区
};

FrameCodec::FrameCodec() : d(std::make_unique<Impl>()) {}
FrameCodec::~FrameCodec() = default;

std::vector<uint8_t> FrameCodec::encode(uint16_t cmdId, const std::vector<uint8_t>& payload) {
    // TODO: COBS 编码 + CRC16
    return {};
}

void FrameCodec::feed(const std::vector<uint8_t>& rawData) {
    // TODO: 帧解析状态机
}

} // namespace MotorStudio