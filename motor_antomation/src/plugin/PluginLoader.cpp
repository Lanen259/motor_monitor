#include "PluginLoader.h"

namespace MotorStudio {

struct PluginLoader::Impl {
    std::vector<LoadedPlugin> plugins;
};

PluginLoader& PluginLoader::instance() {
    static PluginLoader loader;
    return loader;
}

PluginLoader::PluginLoader() : d(std::make_unique<Impl>()) {}
PluginLoader::~PluginLoader() = default;

std::vector<PluginDescriptor> PluginLoader::scanPlugins(const std::string& pluginDir) {
    return {};
}

bool PluginLoader::loadPlugin(const std::string& filePath) {
    return false;
}

void PluginLoader::unloadPlugin(const std::string& pluginName) {}
void PluginLoader::unloadAll() {}

IPlugin* PluginLoader::getPlugin(const std::string& pluginName) const {
    return nullptr;
}

std::vector<const LoadedPlugin*> PluginLoader::loadedPlugins() const {
    return {};
}

std::vector<IPlugin*> PluginLoader::pluginsByType(PluginType type) const {
    return {};
}

size_t PluginLoader::loadedCount() const { return 0; }

} // namespace MotorStudio