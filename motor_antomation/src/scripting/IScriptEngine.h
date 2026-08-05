#pragma once
#include <string>
#include <functional>
#include <memory>

namespace MotorStudio {

// 脚本引擎抽象接口
class IScriptEngine {
public:
    virtual ~IScriptEngine() = default;

    virtual bool initialize() = 0;
    virtual void shutdown() = 0;
    virtual bool isInitialized() const = 0;

    // 执行脚本
    virtual bool execute(const std::string& script) = 0;
    virtual bool executeFile(const std::string& filePath) = 0;

    // 注册 C++ 函数
    using NativeFunction = std::function<void()>;
    virtual void registerFunction(const std::string& name, NativeFunction func) = 0;

    virtual std::string engineName() const = 0;
    virtual std::string engineVersion() const = 0;
};

} // namespace MotorStudio