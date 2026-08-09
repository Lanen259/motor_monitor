#include "MotorProtocol.h"
#include <cstring>
#include <algorithm>
#include <stdexcept>
#include <QDebug>

namespace MotorStudio {

// ============================================================
// CRC16 实现
// ============================================================
bool Crc16::tableInitialized_ = false;
uint16_t Crc16::table_[256];

void Crc16::initTable() {
    if (tableInitialized_) return;
    constexpr uint16_t polynomial = 0x1021;
    for (uint16_t i = 0; i < 256; ++i) {
        uint16_t crc = i << 8;
        for (int j = 0; j < 8; ++j) {
            crc = (crc & 0x8000) ? (crc << 1) ^ polynomial : (crc << 1);
        }
        table_[i] = crc;
    }
    tableInitialized_ = true;
}

uint16_t Crc16::compute(const uint8_t* data, size_t length) {
    initTable();
    uint16_t crc = 0x0000;
    for (size_t i = 0; i < length; ++i) {
        crc = (crc << 8) ^ table_[((crc >> 8) ^ data[i]) & 0xFF];
    }
    return crc;
}

uint16_t Crc16::compute(const std::vector<uint8_t>& data) {
    return compute(data.data(), data.size());
}

bool Crc16::verify(const uint8_t* data, size_t length, uint16_t expectedCrc) {
    return compute(data, length) == expectedCrc;
}

// ============================================================
// MotorProtocol 实现
// ============================================================

MotorProtocol::MotorProtocol()
    : state_(ParseState::WaitSync)
    , expectedLength_(0)
    , currentDevId_(0)
    , currentCmd_(0)
    , currentPayloadLen_(0)
    , framesDecoded_(0)
    , crcErrors_(0)
    , frameErrors_(0)
{
    buffer_.reserve(4096);

    // 运行时验证 Payload 大小（packed 布局：4 + 10*4 + 2 = 46 字节）
    Q_ASSERT(sizeof(MotorDataPayload) == 46);
}

MotorProtocol::~MotorProtocol() = default;

void MotorProtocol::reset() {
    state_ = ParseState::WaitSync;
    buffer_.clear();
    expectedLength_ = 0;
}

// --- 编码 ---

std::vector<uint8_t> MotorProtocol::encodeFrame(uint8_t deviceId, MotorCommand cmd,
                                                 const std::vector<uint8_t>& payload) {
    std::vector<uint8_t> frame;
    uint16_t payloadLen = static_cast<uint16_t>(payload.size());

    // Sync
    frame.push_back(0xAA);
    frame.push_back(0x55);

    // DeviceID
    frame.push_back(deviceId);

    // Command
    frame.push_back(static_cast<uint8_t>(cmd));

    // Length (LE)
    frame.push_back(payloadLen & 0xFF);
    frame.push_back((payloadLen >> 8) & 0xFF);

    // Payload
    frame.insert(frame.end(), payload.begin(), payload.end());

    // CRC16 (over header + payload)
    size_t headerAndPayloadLen = 6 + payloadLen;
    uint16_t crc = Crc16::compute(frame.data(), headerAndPayloadLen);
    frame.push_back(crc & 0xFF);
    frame.push_back((crc >> 8) & 0xFF);

    // Footer
    frame.push_back(0x0D);
    frame.push_back(0x0A);

    return frame;
}

std::vector<uint8_t> MotorProtocol::encodeDataUpload(uint8_t deviceId,
                                                      const MotorDataPayload& payload) {
    std::vector<uint8_t> payBytes(sizeof(MotorDataPayload));
    std::memcpy(payBytes.data(), &payload, sizeof(MotorDataPayload));
    return encodeFrame(deviceId, MotorCommand::DataUpload, payBytes);
}

std::vector<uint8_t> MotorProtocol::encodeCommand(uint8_t deviceId, MotorCommand cmd,
                                                   const std::vector<uint8_t>& cmdPayload) {
    return encodeFrame(deviceId, cmd, cmdPayload);
}

// --- 解码 ---

void MotorProtocol::feed(const std::vector<uint8_t>& rawData) {
    buffer_.insert(buffer_.end(), rawData.begin(), rawData.end());

    while (tryParseFrame()) {
        // 继续尝试解析（可能有多帧）
    }
}

bool MotorProtocol::tryParseFrame() {
    if (buffer_.size() < 2) return false;

    // 查找同步头
    size_t syncPos = 0;
    while (syncPos + 1 < buffer_.size()) {
        if (buffer_[syncPos] == 0xAA && buffer_[syncPos + 1] == 0x55) {
            break;
        }
        ++syncPos;
    }

    if (syncPos > 0) {
        // 丢弃同步头之前的字节
        frameErrors_ += syncPos;
        buffer_.erase(buffer_.begin(), buffer_.begin() + syncPos);
    }

    if (buffer_.size() < 2 || buffer_[0] != 0xAA || buffer_[1] != 0x55) {
        return false;
    }

    // 需要至少 Sync(2) + DevID(1) + Cmd(1) + Len(2) = 6 bytes
    if (buffer_.size() < 6) return false;

    currentDevId_ = buffer_[2];
    currentCmd_ = buffer_[3];
    currentPayloadLen_ = buffer_[4] | (static_cast<uint16_t>(buffer_[5]) << 8);

    size_t totalFrameLen = 6 + currentPayloadLen_ + 2 + 2; // header + payload + crc + footer

    if (buffer_.size() < totalFrameLen) {
        return false; // 数据不完整，等待更多数据
    }

    // 验证 CRC
    uint16_t receivedCrc = buffer_[6 + currentPayloadLen_]
                         | (static_cast<uint16_t>(buffer_[6 + currentPayloadLen_ + 1]) << 8);

    bool crcOk = Crc16::verify(buffer_.data(), 6 + currentPayloadLen_, receivedCrc);

    // 验证 Footer
    bool footerOk = (buffer_[6 + currentPayloadLen_ + 2] == 0x0D)
                 && (buffer_[6 + currentPayloadLen_ + 3] == 0x0A);

    // 提取 payload
    DecodedFrame frame;
    frame.deviceId = currentDevId_;
    frame.command = static_cast<MotorCommand>(currentCmd_);
    frame.crcValid = crcOk;

    if (currentPayloadLen_ > 0) {
        frame.payload.assign(buffer_.begin() + 6,
                             buffer_.begin() + 6 + currentPayloadLen_);
    }

    if (!crcOk) {
        crcErrors_++;
    }
    if (!footerOk) {
        frameErrors_++;
    }

    framesDecoded_++;
    frameQueue_.push_back(std::move(frame));

    // 移除已解析的帧
    buffer_.erase(buffer_.begin(), buffer_.begin() + totalFrameLen);

    return true;
}

bool MotorProtocol::hasFrame() const {
    return !frameQueue_.empty();
}

MotorProtocol::DecodedFrame MotorProtocol::popFrame() {
    if (frameQueue_.empty()) {
        throw std::runtime_error("No frame available");
    }
    DecodedFrame frame = std::move(frameQueue_.front());
    frameQueue_.erase(frameQueue_.begin());
    return frame;
}

bool MotorProtocol::parseDataPayload(const std::vector<uint8_t>& payload, MotorDataPayload& out) {
    if (payload.size() != sizeof(MotorDataPayload)) {
        return false;
    }
    std::memcpy(&out, payload.data(), sizeof(MotorDataPayload));
    return true;
}

} // namespace MotorStudio