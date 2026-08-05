#include "Application.h"

namespace MotorStudio {

struct Application::Impl {
    bool running = false;
};

Application::Application() : d(std::make_unique<Impl>()) {}
Application::~Application() = default;

bool Application::initialize() {
    d->running = true;
    return true;
}

void Application::shutdown() {
    d->running = false;
}

bool Application::isRunning() const {
    return d->running;
}

} // namespace MotorStudio