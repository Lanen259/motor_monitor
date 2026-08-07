#include "AutomationEngine.h"
#include "databus/DataBus.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QEventLoop>
#include <QTimer>
#include <QThread>
#include <QDebug>
#include <chrono>
#include <algorithm>

namespace MotorStudio {

struct AutomationEngine::Impl {
    std::vector<TestCase> testCases;
    TestCase currentTestCase;
    bool running = false;
    bool paused = false;
    bool stopRequested = false;
    int currentStepIndex = 0;

    // Callbacks
    SetParamCallback setParamCb;
    ReadParamCallback readParamCb;
    MotorControlCallback motorStartCb;
    MotorControlCallback motorStopCb;

    // Custom step handlers
    std::unordered_map<std::string, CustomStepFunc> customSteps;

    // Thread synchronization
    mutable QMutex mutex;
    QWaitCondition pauseCondition;
};

AutomationEngine::AutomationEngine(QObject* parent)
    : QObject(parent), d(std::make_unique<Impl>())
{
    qRegisterMetaType<TestResult>("TestResult");
    qRegisterMetaType<std::string>("std::string");
}

AutomationEngine::~AutomationEngine() = default;

// ============================================================
// Callback setters
// ============================================================

void AutomationEngine::setSetParamCallback(SetParamCallback cb) { d->setParamCb = std::move(cb); }
void AutomationEngine::setReadParamCallback(ReadParamCallback cb) { d->readParamCb = std::move(cb); }
void AutomationEngine::setMotorStartCallback(MotorControlCallback cb) { d->motorStartCb = std::move(cb); }
void AutomationEngine::setMotorStopCallback(MotorControlCallback cb) { d->motorStopCb = std::move(cb); }

const TestCase& AutomationEngine::currentTestCase() const { return d->currentTestCase; }

void AutomationEngine::setCurrentTestCase(const TestCase& tc)
{
    QMutexLocker lk(&d->mutex);
    d->currentTestCase = tc;
    d->testCases.clear();
    d->testCases.push_back(tc);
}

// ============================================================
// StepType string parsing
// ============================================================

StepType AutomationEngine::parseStepType(const std::string& str) const
{
    if (str == "SetParameter") return StepType::SetParameter;
    if (str == "Wait")         return StepType::Wait;
    if (str == "ReadParameter") return StepType::ReadParameter;
    if (str == "Assert")       return StepType::Assert;
    if (str == "RecordData")   return StepType::RecordData;
    if (str == "StartMotor")   return StepType::StartMotor;
    if (str == "StopMotor")    return StepType::StopMotor;
    if (str == "Custom")       return StepType::Custom;
    return StepType::Custom; // fallback
}

std::string AutomationEngine::findParam(const TestStep& step, const std::string& key, const std::string& defaultValue) const
{
    for (const auto& kv : step.params) {
        if (kv.first == key) return kv.second;
    }
    return defaultValue;
}

// ============================================================
// JSON loading
// ============================================================

bool AutomationEngine::loadTestCase(const std::string& jsonFilePath)
{
    QFile file(QString::fromStdString(jsonFilePath));
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "AutomationEngine: Cannot open test case file:" << jsonFilePath.c_str();
        return false;
    }

    QByteArray data = file.readAll();
    file.close();

    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        qWarning() << "AutomationEngine: JSON parse error:" << parseError.errorString();
        return false;
    }

    if (!doc.isObject()) {
        qWarning() << "AutomationEngine: JSON root is not an object";
        return false;
    }

    QJsonObject root = doc.object();

    TestCase tc;
    tc.name = root.value("name").toString("Unnamed Test").toStdString();
    tc.description = root.value("description").toString("").toStdString();
    tc.stopOnFailure = root.value("stopOnFailure").toBool(true);

    QJsonArray stepsArray = root.value("steps").toArray();
    if (stepsArray.isEmpty()) {
        qWarning() << "AutomationEngine: No steps defined in test case";
        return false;
    }

    for (const QJsonValue& stepVal : stepsArray) {
        if (!stepVal.isObject()) continue;

        QJsonObject stepObj = stepVal.toObject();
        TestStep step;
        step.type = parseStepType(stepObj.value("type").toString("Custom").toStdString());
        step.description = stepObj.value("description").toString("").toStdString();
        step.timeoutMs = static_cast<uint32_t>(stepObj.value("timeoutMs").toInt(5000));
        step.retryCount = stepObj.value("retryCount").toInt(0);

        QJsonObject paramsObj = stepObj.value("params").toObject();
        for (auto it = paramsObj.begin(); it != paramsObj.end(); ++it) {
            std::string key = it.key().toStdString();
            std::string value;
            if (it.value().isString()) {
                value = it.value().toString().toStdString();
            } else if (it.value().isDouble()) {
                value = std::to_string(it.value().toDouble());
            } else {
                value = it.value().toString().toStdString();
            }
            step.params.emplace_back(key, value);
        }

        tc.steps.push_back(std::move(step));
    }

    d->currentTestCase = std::move(tc);
    d->testCases.clear();
    d->testCases.push_back(d->currentTestCase);

    qDebug() << "AutomationEngine: Loaded test case" << d->currentTestCase.name.c_str()
             << "with" << d->currentTestCase.steps.size() << "steps";
    return true;
}

bool AutomationEngine::loadTestSuite(const std::string& jsonFilePath)
{
    QFile file(QString::fromStdString(jsonFilePath));
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "AutomationEngine: Cannot open test suite file:" << jsonFilePath.c_str();
        return false;
    }

    QByteArray data = file.readAll();
    file.close();

    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        qWarning() << "AutomationEngine: JSON parse error:" << parseError.errorString();
        return false;
    }

    QJsonArray suiteArray;
    if (doc.isArray()) {
        suiteArray = doc.array();
    } else if (doc.isObject()) {
        // Wrap single test case as suite
        suiteArray.append(doc.object());
    } else {
        qWarning() << "AutomationEngine: Invalid test suite format";
        return false;
    }

    d->testCases.clear();
    for (const QJsonValue& caseVal : suiteArray) {
        if (!caseVal.isObject()) continue;
        QJsonObject root = caseVal.toObject();

        TestCase tc;
        tc.name = root.value("name").toString("Unnamed Test").toStdString();
        tc.description = root.value("description").toString("").toStdString();
        tc.stopOnFailure = root.value("stopOnFailure").toBool(true);

        QJsonArray stepsArray = root.value("steps").toArray();
        for (const QJsonValue& stepVal : stepsArray) {
            if (!stepVal.isObject()) continue;
            QJsonObject stepObj = stepVal.toObject();
            TestStep step;
            step.type = parseStepType(stepObj.value("type").toString("Custom").toStdString());
            step.description = stepObj.value("description").toString("").toStdString();
            step.timeoutMs = static_cast<uint32_t>(stepObj.value("timeoutMs").toInt(5000));
            step.retryCount = stepObj.value("retryCount").toInt(0);

            QJsonObject paramsObj = stepObj.value("params").toObject();
            for (auto it = paramsObj.begin(); it != paramsObj.end(); ++it) {
                std::string value;
                if (it.value().isString())
                    value = it.value().toString().toStdString();
                else if (it.value().isDouble())
                    value = std::to_string(it.value().toDouble());
                else
                    value = it.value().toString().toStdString();
                step.params.emplace_back(it.key().toStdString(), value);
            }
            tc.steps.push_back(std::move(step));
        }
        d->testCases.push_back(std::move(tc));
    }

    if (!d->testCases.empty()) {
        d->currentTestCase = d->testCases[0];
    }

    qDebug() << "AutomationEngine: Loaded test suite with" << d->testCases.size() << "cases";
    return true;
}

// ============================================================
// Main run loop (intended to be called from worker thread)
// ============================================================

void AutomationEngine::run()
{
    QMutexLocker locker(&d->mutex);

    if (d->currentTestCase.steps.empty()) {
        qWarning() << "AutomationEngine: No test case loaded";
        locker.unlock();
        emit testCompleted(TestResult{false, "No test case loaded", "No steps", -1, std::chrono::milliseconds(0), {}});
        return;
    }

    d->running = true;
    d->paused = false;
    d->stopRequested = false;
    d->currentStepIndex = 0;

    const int totalSteps = static_cast<int>(d->currentTestCase.steps.size());
    const auto& testCase = d->currentTestCase;
    const bool stopOnFailure = testCase.stopOnFailure;

    locker.unlock();

    emit testStarted(testCase.name);

    auto startTime = std::chrono::steady_clock::now();
    TestResult result;
    result.caseName = testCase.name;
    result.passed = true;

    for (int i = 0; i < totalSteps; ++i) {
        // Check for stop request
        {
            QMutexLocker lk(&d->mutex);
            if (d->stopRequested) {
                result.passed = false;
                result.errorMessage = "Test stopped by user";
                result.failedStepIndex = i;
                lk.unlock();
                emit logMessage("Test stopped by user at step " + std::to_string(i));
                break;
            }

            // Handle pause
            while (d->paused && !d->stopRequested) {
                lk.unlock();
                emit logMessage("Test paused at step " + std::to_string(i));
                // Process events while paused so stop() signal can be delivered
                QEventLoop pauseLoop;
                QTimer pauseTimer;
                pauseTimer.setInterval(100);
                QObject::connect(&pauseTimer, &QTimer::timeout, &pauseLoop, [&]() {
                    QMutexLocker innerLk(&d->mutex);
                    if (!d->paused || d->stopRequested) {
                        pauseLoop.quit();
                    }
                });
                pauseTimer.start();
                pauseLoop.exec();
                pauseTimer.stop();
                lk.relock();
            }
        }

        const TestStep& step = testCase.steps[i];
        d->currentStepIndex = i;

        emit stepStarted(i, step.description);
        emit progressUpdated(i + 1, totalSteps);
        emit logMessage("Step " + std::to_string(i + 1) + "/" + std::to_string(totalSteps)
                        + ": " + step.description);

        bool stepSuccess = false;
        int retries = step.retryCount + 1; // at least one attempt

        for (int attempt = 0; attempt < retries; ++attempt) {
            if (attempt > 0) {
                emit logMessage("  Retry " + std::to_string(attempt) + "/" + std::to_string(retries - 1));
            }
            stepSuccess = executeStep(step);
            if (stepSuccess) break;

            // Check stop between retries
            QMutexLocker lk(&d->mutex);
            if (d->stopRequested) {
                result.passed = false;
                result.errorMessage = "Test stopped during retry";
                result.failedStepIndex = i;
                lk.unlock();
                break;
            }
        }

        emit stepCompleted(i, stepSuccess);
        result.logs.push_back((stepSuccess ? "[PASS] " : "[FAIL] ") + step.description);

        if (!stepSuccess) {
            result.passed = false;
            result.errorMessage = "Step " + std::to_string(i) + " failed: " + step.description;
            result.failedStepIndex = i;

            if (stopOnFailure) {
                emit logMessage("Test stopped due to step failure (stopOnFailure=true)");
                break;
            }
        }
    }

    auto endTime = std::chrono::steady_clock::now();
    result.duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);

    {
        QMutexLocker lk(&d->mutex);
        d->running = false;
        d->paused = false;
        d->stopRequested = false;
    }

    emit logMessage(result.passed ? "Test PASSED" : "Test FAILED: " + result.errorMessage);
    emit progressUpdated(totalSteps, totalSteps);
    emit testCompleted(result);
}

// ============================================================
// Step execution dispatch
// ============================================================

bool AutomationEngine::executeStep(const TestStep& step)
{
    switch (step.type) {
    case StepType::SetParameter: {
        if (!d->setParamCb) {
            emit logMessage("  SetParam callback not registered");
            return false;
        }
        // Each param key-value pair is set individually
        for (const auto& kv : step.params) {
            if (!d->setParamCb(kv.first, kv.second)) {
                emit logMessage("  SetParam failed: " + kv.first + " = " + kv.second);
                return false;
            }
            emit logMessage("  SetParam: " + kv.first + " = " + kv.second);
        }
        // Let event loop process pending
        QThread::msleep(10);
        return true;
    }

    case StepType::Wait: {
        std::string durStr = findParam(step, "durationMs", "0");
        int durationMs = 0;
        try { durationMs = std::stoi(durStr); } catch (...) { durationMs = 0; }
        if (durationMs <= 0) {
            emit logMessage("  Wait: invalid duration, skipping");
            return true;
        }

        // Non-blocking wait using QEventLoop + QTimer (works on any thread with event loop)
        emit logMessage("  Waiting " + std::to_string(durationMs) + "ms...");
        QEventLoop waitLoop;
        QTimer::singleShot(durationMs, &waitLoop, &QEventLoop::quit);

        // Also check for stop during wait
        QTimer stopCheck;
        stopCheck.setInterval(50);
        bool stopped = false;
        QObject::connect(&stopCheck, &QTimer::timeout, &waitLoop, [&]() {
            QMutexLocker lk(&d->mutex);
            if (d->stopRequested) {
                stopped = true;
                waitLoop.quit();
            }
        });
        stopCheck.start();

        // Timeout safety: use the step's timeout
        int effectiveTimeout = (step.timeoutMs > 0 && static_cast<int>(step.timeoutMs) > durationMs)
                                ? static_cast<int>(step.timeoutMs) : durationMs + 2000;
        QTimer::singleShot(effectiveTimeout, &waitLoop, [&]() {
            stopped = true;
            waitLoop.quit();
            emit logMessage("  Wait: timeout exceeded");
        });

        waitLoop.exec();
        stopCheck.stop();

        if (stopped) return false;
        emit logMessage("  Wait complete");
        return true;
    }

    case StepType::ReadParameter: {
        if (!d->readParamCb) {
            emit logMessage("  ReadParam callback not registered");
            return false;
        }
        std::string paramName = findParam(step, "name", "");
        if (paramName.empty()) {
            emit logMessage("  ReadParam: no parameter name specified");
            return false;
        }
        std::string value = d->readParamCb(paramName);
        emit logMessage("  ReadParam: " + paramName + " = " + value);
        return true;
    }

    case StepType::Assert: {
        std::string channel = findParam(step, "channel", "");
        std::string minStr = findParam(step, "min", "");
        std::string maxStr = findParam(step, "max", "");

        if (channel.empty()) {
            emit logMessage("  Assert: no channel specified");
            return false;
        }

        // Look up channel by name in TopicRegistry
        auto& registry = TopicRegistry::instance();
        TopicId tid = registry.findTopic(channel);
        if (tid == 0) {
            emit logMessage("  Assert: channel '" + channel + "' not found in registry");
            return false;
        }

        auto optValue = DataBus::instance().latestValue(tid);
        if (!optValue.has_value()) {
            emit logMessage("  Assert: no data for channel '" + channel + "'");
            return false;
        }

        float value = optValue.value();

        bool ok = true;
        if (!minStr.empty()) {
            try {
                float minVal = std::stof(minStr);
                if (value < minVal) {
                    emit logMessage("  Assert FAIL: " + channel + " = " + std::to_string(value)
                                    + " < min(" + minStr + ")");
                    ok = false;
                }
            } catch (...) {
                emit logMessage("  Assert: invalid min value '" + minStr + "'");
                ok = false;
            }
        }
        if (ok && !maxStr.empty()) {
            try {
                float maxVal = std::stof(maxStr);
                if (value > maxVal) {
                    emit logMessage("  Assert FAIL: " + channel + " = " + std::to_string(value)
                                    + " > max(" + maxStr + ")");
                    ok = false;
                }
            } catch (...) {
                emit logMessage("  Assert: invalid max value '" + maxStr + "'");
                ok = false;
            }
        }

        if (ok) {
            emit logMessage("  Assert PASS: " + channel + " = " + std::to_string(value)
                            + " in [" + minStr + ", " + maxStr + "]");
        }
        return ok;
    }

    case StepType::StartMotor: {
        if (!d->motorStartCb) {
            emit logMessage("  StartMotor callback not registered");
            return false;
        }
        bool ok = d->motorStartCb();
        emit logMessage(ok ? "  StartMotor: OK" : "  StartMotor: FAILED");
        return ok;
    }

    case StepType::StopMotor: {
        if (!d->motorStopCb) {
            emit logMessage("  StopMotor callback not registered");
            return false;
        }
        bool ok = d->motorStopCb();
        emit logMessage(ok ? "  StopMotor: OK" : "  StopMotor: FAILED");
        return ok;
    }

    case StepType::RecordData: {
        // Append current DataBus snapshot to internal log with timestamp
        auto now = std::chrono::system_clock::now();
        auto timeT = std::chrono::system_clock::to_time_t(now);
        std::string ts = std::ctime(&timeT);
        if (!ts.empty() && ts.back() == '\n') ts.pop_back();

        auto& registry = TopicRegistry::instance();
        auto& bus = DataBus::instance();
        auto allIds = registry.allTopicIds();

        std::string record = "[" + ts + "] ";
        for (auto tid : allIds) {
            auto v = bus.latestValue(tid);
            if (v.has_value()) {
                record += registry.topicName(tid) + "=" + std::to_string(v.value()) + " ";
            }
        }
        emit logMessage("  RecordData: " + record);
        return true;
    }

    case StepType::Custom: {
        std::string handlerName = findParam(step, "handler", "");
        if (handlerName.empty()) {
            emit logMessage("  Custom step: no handler specified");
            return false;
        }
        auto it = d->customSteps.find(handlerName);
        if (it == d->customSteps.end()) {
            emit logMessage("  Custom step: handler '" + handlerName + "' not registered");
            return false;
        }
        bool ok = it->second(step);
        emit logMessage(ok ? "  Custom step '" + handlerName + "': OK"
                            : "  Custom step '" + handlerName + "': FAILED");
        return ok;
    }
    }

    return false;
}

// ============================================================
// Control
// ============================================================

void AutomationEngine::stop()
{
    QMutexLocker lk(&d->mutex);
    d->stopRequested = true;
    d->paused = false;
    d->pauseCondition.wakeAll();
}

void AutomationEngine::pause()
{
    QMutexLocker lk(&d->mutex);
    d->paused = true;
}

void AutomationEngine::resume()
{
    QMutexLocker lk(&d->mutex);
    d->paused = false;
    d->pauseCondition.wakeAll();
}

bool AutomationEngine::isRunning() const
{
    QMutexLocker lk(&d->mutex);
    return d->running;
}

bool AutomationEngine::isPaused() const
{
    QMutexLocker lk(&d->mutex);
    return d->paused;
}

void AutomationEngine::registerCustomStep(const std::string& name, CustomStepFunc func)
{
    QMutexLocker lk(&d->mutex);
    d->customSteps[name] = std::move(func);
}

} // namespace MotorStudio
