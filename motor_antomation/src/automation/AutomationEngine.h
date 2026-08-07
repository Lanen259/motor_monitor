#pragma once
#include <QObject>
#include <QMutex>
#include <QWaitCondition>
#include <memory>
#include <string>
#include <vector>
#include <functional>
#include <chrono>
#include <unordered_map>

namespace MotorStudio {

// 测试步骤类型
enum class StepType : uint8_t {
    SetParameter,       // 设置参数
    Wait,               // 等待
    ReadParameter,      // 读取参数
    Assert,             // 断言
    RecordData,         // 记录数据
    StartMotor,         // 启动电机
    StopMotor,          // 停止电机
    Custom              // 自定义步骤
};

// 测试步骤
struct TestStep {
    StepType type;
    std::string description;
    std::vector<std::pair<std::string, std::string>> params;  // 参数键值对
    uint32_t timeoutMs = 5000;
    int retryCount = 0;
};

// 测试用例
struct TestCase {
    std::string name;
    std::string description;
    std::vector<TestStep> steps;
    bool stopOnFailure = true;
};

// 测试结果
struct TestResult {
    bool passed = false;
    std::string caseName;
    std::string errorMessage;
    int failedStepIndex = -1;
    std::chrono::milliseconds duration;
    std::vector<std::string> logs;
};

// 自动化测试引擎
class AutomationEngine : public QObject {
    Q_OBJECT
public:
    explicit AutomationEngine(QObject* parent = nullptr);
    ~AutomationEngine() override;

    // 加载测试用例
    bool loadTestCase(const std::string& jsonFilePath);
    bool loadTestSuite(const std::string& jsonFilePath);

    // 运行测试（在工作线程中调用）
    Q_INVOKABLE void run();
    void stop();
    void pause();
    void resume();
    bool isRunning() const;
    bool isPaused() const;

    // 步骤执行（供脚本调用）
    bool executeStep(const TestStep& step);

    // 注册自定义步骤
    using CustomStepFunc = std::function<bool(const TestStep&)>;
    void registerCustomStep(const std::string& name, CustomStepFunc func);

    // 回调注册（从 MainWindow 注册，连接到 ParameterManager）
    using SetParamCallback = std::function<bool(const std::string& name, const std::string& value)>;
    using ReadParamCallback = std::function<std::string(const std::string& name)>;
    using MotorControlCallback = std::function<bool()>;

    void setSetParamCallback(SetParamCallback cb);
    void setReadParamCallback(ReadParamCallback cb);
    void setMotorStartCallback(MotorControlCallback cb);
    void setMotorStopCallback(MotorControlCallback cb);

    // 获取当前测试用例（只读）
    const TestCase& currentTestCase() const;
    // 设置当前测试用例（供TestRunner调用）
    void setCurrentTestCase(const TestCase& tc);

signals:
    void testStarted(const std::string& caseName);
    void testCompleted(const TestResult& result);
    void stepStarted(int stepIndex, const std::string& description);
    void stepCompleted(int stepIndex, bool success);
    void progressUpdated(int current, int total);
    void logMessage(const std::string& message);

private:
    StepType parseStepType(const std::string& str) const;
    std::string findParam(const TestStep& step, const std::string& key, const std::string& defaultValue = "") const;

    struct Impl;
    std::unique_ptr<Impl> d;
};

} // namespace MotorStudio

Q_DECLARE_METATYPE(MotorStudio::TestResult)
Q_DECLARE_METATYPE(std::string)
