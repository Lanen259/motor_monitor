#include "LuaEngine.h"

namespace MotorStudio {

struct LuaEngine::Impl {
    bool initialized = false;
};

LuaEngine::LuaEngine() : d(std::make_unique<Impl>()) {}
LuaEngine::~LuaEngine() = default;

bool LuaEngine::initialize() {
    d->initialized = true;
    return true;
}

void LuaEngine::shutdown() {
    d->initialized = false;
}

bool LuaEngine::isInitialized() const {
    return d->initialized;
}

bool LuaEngine::execute(const std::string& script) {
    return false;
}

bool LuaEngine::executeFile(const std::string& filePath) {
    return false;
}

void LuaEngine::registerFunction(const std::string& name, NativeFunction func) {}

} // namespace MotorStudio