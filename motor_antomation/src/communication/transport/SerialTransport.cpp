#include "SerialTransport.h"

namespace MotorStudio {

struct SerialTransport::Impl {
    bool open = false;
};

SerialTransport::SerialTransport(QObject* parent) : ITransport(parent), d(std::make_unique<Impl>()) {}
SerialTransport::~SerialTransport() = default;

bool SerialTransport::open(const std::string& config) {
    // TODO: 实现串口打开
    d->open = true;
    emit connected();
    return true;
}

void SerialTransport::close() {
    d->open = false;
    emit disconnected();
}

bool SerialTransport::isOpen() const {
    return d->open;
}

bool SerialTransport::send(const QByteArray& data) {
    if (!d->open) return false;
    // TODO: 实现异步发送
    return true;
}

} // namespace MotorStudio