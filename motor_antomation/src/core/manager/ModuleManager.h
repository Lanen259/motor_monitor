#pragma once
#include <QObject>
#include <memory>
#include <vector>
#include <string>

namespace MotorStudio {

// 模块基类
class IModule {
public:
    virtual ~IModule() = default;
    virtual bool initialize() = 0;
    virtual void shutdown() = 0;
    virtual const char* name() const = 0;
};

// 模块管理器（管理所有模块生命周期）
class ModuleManager : public QObject {
    Q_OBJECT
public:
    static ModuleManager& instance();

    void registerModule(std::unique_ptr<IModule> module);
    bool initializeAll();
    void shutdownAll();

private:
    ModuleManager() = default;
    std::vector<std::unique_ptr<IModule>> modules_;
};

} // namespace MotorStudio