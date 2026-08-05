#include "UdpTransport.h"

namespace MotorStudio {

struct UdpTransport::Impl {
    bool open = false;
};

UdpTransport::UdpTransport(QObject* parent) : ITransport(parent), d(std::make_unique<Impl>()) {}
UdpTransport::~UdpTransport() = default;

bool UdpTransport::open(const std::string& config) {
    d->open = true;
    emit connected();
    return true;
}

void UdpTransport::close() {
    d->open = false;
    emit disconnected();
}

bool UdpTransport::isOpen() const {
    return d->open;
}

bool UdpTransport::send(const QByteArray& data) {
    if (!d->open) return false;
    return true;
}

} // namespace MotorStudio