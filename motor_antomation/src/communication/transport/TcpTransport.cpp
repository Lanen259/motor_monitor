#include "TcpTransport.h"

namespace MotorStudio {

struct TcpTransport::Impl {
    bool open = false;
};

TcpTransport::TcpTransport(QObject* parent) : ITransport(parent), d(std::make_unique<Impl>()) {}
TcpTransport::~TcpTransport() = default;

bool TcpTransport::open(const std::string& config) {
    d->open = true;
    emit connected();
    return true;
}

void TcpTransport::close() {
    d->open = false;
    emit disconnected();
}

bool TcpTransport::isOpen() const {
    return d->open;
}

bool TcpTransport::send(const QByteArray& data) {
    if (!d->open) return false;
    return true;
}

} // namespace MotorStudio