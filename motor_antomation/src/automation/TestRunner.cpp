#include "TestRunner.h"

namespace MotorStudio {

struct TestRunner::Impl {
    AutomationEngine* engine = nullptr;
    bool running = false;
};

TestRunner::TestRunner(AutomationEngine* engine, QObject* parent)
    : QObject(parent), d(std::make_unique<Impl>()) {
    d->engine = engine;
}

TestRunner::~TestRunner() = default;

void TestRunner::runAsync(const TestCase& testCase) {
    d->running = true;
}

void TestRunner::stop() {
    d->running = false;
}

bool TestRunner::isRunning() const { return d->running; }

} // namespace MotorStudio