#pragma once
#include "IScriptEngine.h"
#include <memory>

namespace MotorStudio {

class LuaEngine : public IScriptEngine {
public:
    LuaEngine();
    ~LuaEngine() override;

    bool initialize() override;
    void shutdown() override;
    bool isInitialized() const override;

    bool execute(const std::string& script) override;
    bool executeFile(const std::string& filePath) override;

    void registerFunction(const std::string& name, NativeFunction func) override;

    std::string engineName() const override { return "Lua"; }
    std::string engineVersion() const override { return "5.4"; }

private:
    struct Impl;
    std::unique_ptr<Impl> d;
};

} // namespace MotorStudio