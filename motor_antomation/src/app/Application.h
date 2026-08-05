#pragma once
#include <memory>

namespace MotorStudio {

class Application {
public:
    Application();
    ~Application();

    bool initialize();
    void shutdown();
    bool isRunning() const;

private:
    struct Impl;
    std::unique_ptr<Impl> d;
};

} // namespace MotorStudio