#pragma once
#include <QObject>
#include <QThread>
#include <QString>
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

    // 已生成的最新报告路径（HTML + CSV）
    QString lastHtmlReport() const;
    QString lastCsvReport() const;

signals:
    void runnerFinished(const TestResult& result);
    // 报告生成完成后发出
    void reportGenerated(const QString& htmlPath, const QString& csvPath);

private slots:
    void onEngineTestCompleted(const TestResult& result);

private:
    struct Impl;
    std::unique_ptr<Impl> d;
};

} // namespace MotorStudio
