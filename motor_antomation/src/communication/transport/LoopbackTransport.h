#pragma once
#include "ITransport.h"
#include <memory>

namespace MotorStudio {

// 回环传输层 —— 用于内部仿真
// 发送的数据直接通过 dataReceived 信号返回
// 模拟零延迟传输，无需外部设备
class LoopbackTransport : public ITransport {
    Q_OBJECT
public:
    explicit LoopbackTransport(QObject* parent = nullptr);
    ~LoopbackTransport() override;

    bool open(const std::string& config) override;
    void close() override;
    bool isOpen() const override;
    bool send(const QByteArray& data) override;
    std::string transportType() const override { return "Loopback"; }

    // 模拟延迟（毫秒），默认 0
    void setSimulatedDelay(int ms);

    // 模拟丢包率（0.0 - 1.0），默认 0
    void setPacketLossRate(float rate);

    // 统计
    size_t bytesSent() const;
    size_t bytesReceived() const;
    size_t packetsSent() const;
    size_t packetsReceived() const;
    size_t packetsLost() const;

private:
    struct Impl;
    std::unique_ptr<Impl> d;
};

} // namespace MotorStudio