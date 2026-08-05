#pragma once
#include <QObject>
#include <memory>
#include <vector>
#include <string>
#include "IPlugin.h"

namespace MotorStudio {

// 已加载插件
struct LoadedPlugin {
    std::string filePath;
    std::unique_ptr<IPlugin> instance;
    void* handle = nullptr;  // 动态库句柄
};

// 插件加载器
class PluginLoader : public QObject {
    Q_OBJECT
public:
    static PluginLoader& instance();
    ~PluginLoader();

    // 扫描插件目录
    std::vector<PluginDescriptor> scanPlugins(const std::string& pluginDir);

    // 加载/卸载插件
    bool loadPlugin(const std::string& filePath);
    void unloadPlugin(const std::string& pluginName);
    void unloadAll();

    // 查询已加载插件
    IPlugin* getPlugin(const std::string& pluginName) const;
    std::vector<const LoadedPlugin*> loadedPlugins() const;
    std::vector<IPlugin*> pluginsByType(PluginType type) const;

    size_t loadedCount() const;

signals:
    void pluginLoaded(const std::string& pluginName);
    void pluginUnloaded(const std::string& pluginName);
    void pluginLoadFailed(const std::string& filePath, const std::string& error);

private:
    PluginLoader();
    struct Impl;
    std::unique_ptr<Impl> d;
};

} // namespace MotorStudio