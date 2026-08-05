#pragma once
#include <string>
#include <cstdint>

// 插件接口（C ABI 兼容，便于动态加载）

#ifdef _WIN32
    #define PLUGIN_EXPORT __declspec(dllexport)
#else
    #define PLUGIN_EXPORT __attribute__((visibility("default")))
#endif

namespace MotorStudio {

// 插件类型
enum class PluginType : uint8_t {
    Transport,      // 传输层插件
    Protocol,       // 协议插件
    Automation,     // 自动化步骤插件
    Curve,          // 曲线分析插件
    UI,             // UI 面板插件
    Script,         // 脚本引擎插件
    Custom          // 自定义插件
};

// 插件版本
struct PluginVersion {
    uint16_t major;
    uint16_t minor;
    uint16_t patch;
};

// 插件描述
struct PluginDescriptor {
    std::string name;
    std::string description;
    PluginType type;
    PluginVersion version;
    PluginVersion minApiVersion;
    std::string author;
};

// 插件基类
class IPlugin {
public:
    virtual ~IPlugin() = default;

    virtual const PluginDescriptor& descriptor() const = 0;
    virtual bool initialize() = 0;
    virtual void shutdown() = 0;
    virtual bool isInitialized() const = 0;
};

// 导出函数（每个插件 DLL 必须实现）
extern "C" {
    PLUGIN_EXPORT IPlugin* createPlugin();
    PLUGIN_EXPORT void destroyPlugin(IPlugin* plugin);
    PLUGIN_EXPORT const char* pluginApiVersion();
}

} // namespace MotorStudio