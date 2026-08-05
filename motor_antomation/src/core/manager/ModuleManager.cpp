#include "ModuleManager.h"

namespace MotorStudio {

ModuleManager& ModuleManager::instance() {
    static ModuleManager mgr;
    return mgr;
}

void ModuleManager::registerModule(std::unique_ptr<IModule> module) {
    modules_.push_back(std::move(module));
}

bool ModuleManager::initializeAll() {
    for (auto& m : modules_) {
        if (!m->initialize()) return false;
    }
    return true;
}

void ModuleManager::shutdownAll() {
    for (auto it = modules_.rbegin(); it != modules_.rend(); ++it) {
        (*it)->shutdown();
    }
}

} // namespace MotorStudio