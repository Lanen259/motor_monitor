#include "AutomationEngine.h"

namespace MotorStudio {

struct AutomationEngine::Impl {
    std::vector<TestCase> testCases;
    bool running = false;
};

AutomationEngine::AutomationEngine(QObject* parent)
    : QObject(parent), d(std::make_unique<Impl>()) {}

AutomationEngine::~AutomationEngine() = default;

bool AutomationEngine::loadTestCase(const std::string& jsonFilePath) {
    return false;
}

bool AutomationEngine::loadTestSuite(const std::string& jsonFilePath) {
    return false;
}

void AutomationEngine::run() {
    d->running = true;
}

void AutomationEngine::stop() {
    d->running = false;
}

void AutomationEngine::pause() {}
void AutomationEngine::resume() {}
bool AutomationEngine::isRunning() const { return d->running; }

bool AutomationEngine::executeStep(const TestStep& step) {
    return false;
}

void AutomationEngine::registerCustomStep(const std::string& name, CustomStepFunc func) {}

} // namespace MotorStudio