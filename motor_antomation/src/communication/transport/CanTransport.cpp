#include "CanTransport.h"

namespace MotorStudio {

struct CanTransport::Impl {
    bool open = false;
};

CanTransport::CanTransport(QObject* parent) : ITransport(parent), d(std::make_unique<Impl>()) {}
CanTransport::~CanTransport() = default;

bool CanTransport::open(const std::string& config) {
    d->open = true;
    emit connected();
    return true;
}

void CanTransport::close() {
    d->open = false;
    emit disconnected();
}

bool CanTransport::isOpen() const {
    return d->open;
}

bool CanTransport::send(const QByteArray& data) {
    if (!d->open) return false;
    return true;
}

} // namespace MotorStudio