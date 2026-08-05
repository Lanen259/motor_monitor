#include "LoopbackTransport.h"
#include <QTimer>
#include <random>
#include <atomic>

namespace MotorStudio {

struct LoopbackTransport::Impl {
    bool open = false;
    int simulatedDelayMs = 0;   // 模拟延迟
    float packetLossRate = 0.0f; // 丢包率
    QTimer* delayTimer = nullptr;

    std::atomic<size_t> bytesSent_{0};
    std::atomic<size_t> bytesReceived_{0};
    std::atomic<size_t> packetsSent_{0};
    std::atomic<size_t> packetsReceived_{0};
    std::atomic<size_t> packetsLost_{0};

    std::mt19937 rng{std::random_device{}()};
    std::uniform_real_distribution<float> dist{0.0f, 1.0f};
};

LoopbackTransport::LoopbackTransport(QObject* parent)
    : ITransport(parent), d(std::make_unique<Impl>())
{
    d->delayTimer = new QTimer(this);
    d->delayTimer->setSingleShot(true);
}

LoopbackTransport::~LoopbackTransport() = default;

bool LoopbackTransport::open(const std::string& /*config*/) {
    d->open = true;
    emit connected();
    return true;
}

void LoopbackTransport::close() {
    d->open = false;
    emit disconnected();
}

bool LoopbackTransport::isOpen() const {
    return d->open;
}

bool LoopbackTransport::send(const QByteArray& data) {
    if (!d->open) return false;

    d->packetsSent_++;
    d->bytesSent_ += data.size();

    // 模拟丢包
    if (d->packetLossRate > 0.0f) {
        float roll = d->dist(d->rng);
        if (roll < d->packetLossRate) {
            d->packetsLost_++;
            return true; // 模拟发送成功，但实际丢弃
        }
    }

    if (d->simulatedDelayMs > 0) {
        // 延迟发送
        QByteArray dataCopy = data;
        QTimer::singleShot(d->simulatedDelayMs, this, [this, dataCopy]() {
            d->bytesReceived_ += dataCopy.size();
            d->packetsReceived_++;
            emit dataReceived(dataCopy);
        });
    } else {
        // 零延迟：直接回环
        d->bytesReceived_ += data.size();
        d->packetsReceived_++;
        emit dataReceived(data);
    }

    return true;
}

void LoopbackTransport::setSimulatedDelay(int ms) {
    d->simulatedDelayMs = ms;
}

void LoopbackTransport::setPacketLossRate(float rate) {
    d->packetLossRate = std::clamp(rate, 0.0f, 1.0f);
}

size_t LoopbackTransport::bytesSent() const { return d->bytesSent_; }
size_t LoopbackTransport::bytesReceived() const { return d->bytesReceived_; }
size_t LoopbackTransport::packetsSent() const { return d->packetsSent_; }
size_t LoopbackTransport::packetsReceived() const { return d->packetsReceived_; }
size_t LoopbackTransport::packetsLost() const { return d->packetsLost_; }

} // namespace MotorStudio