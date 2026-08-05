#pragma once
#include <QObject>
#include <memory>
#include <string>
#include "AutomationEngine.h"

namespace MotorStudio {

// 测试运行器（管理自动化测试线程）
class TestRunner : public QObject {
    Q_OBJECT
public:
    explicit TestRunner(AutomationEngine* engine, QObject* parent = nullptr);
    ~TestRunner() override;

    void runAsync(const TestCase& testCase);
    void stop();
    bool isRunning() const;

signals:
    void runnerFinished(const TestResult& result);

private:
    struct Impl;
    std::unique_ptr<Impl> d;
};

} // namespace MotorStudio