#include <QtTest/QtTest>
#include "../../src/automation/AutomationEngine.h"
#include "../../src/automation/TestRunner.h"
#include "../../src/databus/DataBus.h"
#include "../../src/databus/Topic.h"

using namespace MotorStudio;

// ============================================================================
// TestAutomationEngine — unit tests for AutomationEngine and TestRunner
// ============================================================================

class TestAutomationEngine : public QObject {
    Q_OBJECT

private:
    // Path to the sample JSON test case relative to test executable CWD
    QString sampleJsonPath;
    QString emptyJsonPath;

private slots:
    // ------------------------------------------------------------------------
    // Setup / Teardown
    // ------------------------------------------------------------------------
    void initTestCase()
    {
        // These files are one level up from the build tree unit/automation/ directory
        // In the build tree, the test executable runs with CWD at the build root.
        // We resolve paths relative to the source tree via TEST_DATA_DIR if set,
        // otherwise fall back to a relative path from the tests/ directory.
        const char* dataDir = qgetenv("TEST_DATA_DIR");
        if (dataDir && *dataDir) {
            sampleJsonPath = QString(dataDir) + "/automation/sample_test.json";
            emptyJsonPath  = QString(dataDir) + "/automation/empty_steps.json";
        } else {
            sampleJsonPath = QTest::qFindTestData("sample_test.json");
            emptyJsonPath  = QTest::qFindTestData("empty_steps.json");
        }

        // Pre-register topics that the sample test case references
        auto& reg = TopicRegistry::instance();
        reg.registerTopic("Speed");

        // Register TestResult for QSignalSpy QVariant marshalling
        qRegisterMetaType<TestResult>("TestResult");
    }

    // ------------------------------------------------------------------------
    // AC-1: loadTestCase parses valid JSON, returns true when non-empty
    // ------------------------------------------------------------------------
    void testLoadValidJson()
    {
        AutomationEngine engine;
        QVERIFY(!sampleJsonPath.isEmpty());

        bool ok = engine.loadTestCase(sampleJsonPath.toStdString());
        QVERIFY(ok);

        const auto& tc = engine.currentTestCase();
        QCOMPARE(QString::fromStdString(tc.name), QString("Motor Basic Test"));
        QCOMPARE(QString::fromStdString(tc.description), QString("Basic motor start/stop and parameter validation test"));
        QCOMPARE(tc.stopOnFailure, true);
        QCOMPARE(static_cast<int>(tc.steps.size()), 5);
    }

    // ------------------------------------------------------------------------
    // AC-1 (border): loadTestCase returns false for nonexistent file
    // ------------------------------------------------------------------------
    void testLoadInvalidPath()
    {
        AutomationEngine engine;
        bool ok = engine.loadTestCase("nonexistent_file.json");
        QVERIFY(!ok);
    }

    // ------------------------------------------------------------------------
    // AC-1 (border): loadTestCase returns false for empty steps array
    // ------------------------------------------------------------------------
    void testLoadEmptySteps()
    {
        AutomationEngine engine;
        QVERIFY(!emptyJsonPath.isEmpty());

        bool ok = engine.loadTestCase(emptyJsonPath.toStdString());
        QVERIFY(!ok);  // steps array is empty — must fail
    }

    // ------------------------------------------------------------------------
    // AC-2: JSON format with name, description, steps[]
    // ------------------------------------------------------------------------
    void testLoadedTestCaseStructure()
    {
        AutomationEngine engine;
        bool ok = engine.loadTestCase(sampleJsonPath.toStdString());
        QVERIFY(ok);

        const auto& tc = engine.currentTestCase(); (void) tc;

        // Verify steps structure
        const auto& steps = engine.currentTestCase().steps;
        QCOMPARE(static_cast<int>(steps.size()), 5);

        // Step 0: SetParameter
        QCOMPARE(steps[0].type, StepType::SetParameter);
        QCOMPARE(QString::fromStdString(steps[0].description),
                 QString("Set target speed to 1000 RPM"));
        QCOMPARE(steps[0].timeoutMs, 2000u);
        QVERIFY(steps[0].params.size() >= 1);

        // Step 1: Wait
        QCOMPARE(steps[1].type, StepType::Wait);
        QCOMPARE(steps[1].timeoutMs, 5000u);

        // Step 2: StartMotor
        QCOMPARE(steps[2].type, StepType::StartMotor);
        QCOMPARE(steps[2].timeoutMs, 3000u);

        // Step 3: Assert
        QCOMPARE(steps[3].type, StepType::Assert);

        // Step 4: StopMotor
        QCOMPARE(steps[4].type, StepType::StopMotor);
    }

    // ------------------------------------------------------------------------
    // AC-4: executeStep SetParameter calls callback with correct name/value
    // ------------------------------------------------------------------------
    void testExecuteStepSetParameter()
    {
        AutomationEngine engine;

        bool callbackCalled = false;
        std::string capturedName;
        std::string capturedValue;

        engine.setSetParamCallback([&](const std::string& name, const std::string& value) {
            callbackCalled = true;
            capturedName = name;
            capturedValue = value;
            return true;
        });

        TestStep step;
        step.type = StepType::SetParameter;
        step.description = "Set Speed to 1000";
        step.params = {{"Speed", "1000"}};

        bool ok = engine.executeStep(step);
        QVERIFY(ok);
        QVERIFY(callbackCalled);
        QCOMPARE(QString::fromStdString(capturedName), QString("Speed"));
        QCOMPARE(QString::fromStdString(capturedValue), QString("1000"));
    }

    // ------------------------------------------------------------------------
    // AC-4: executeStep SetParameter fails when no callback registered
    // ------------------------------------------------------------------------
    void testExecuteStepSetParameterNoCallback()
    {
        AutomationEngine engine;

        TestStep step;
        step.type = StepType::SetParameter;
        step.params = {{"Speed", "1000"}};

        bool ok = engine.executeStep(step);
        QVERIFY(!ok);  // callback not registered
    }

    // ------------------------------------------------------------------------
    // AC-4: executeStep StartMotor calls callback
    // ------------------------------------------------------------------------
    void testExecuteStepStartMotor()
    {
        AutomationEngine engine;

        bool startCalled = false;
        engine.setMotorStartCallback([&]() {
            startCalled = true;
            return true;
        });

        TestStep step;
        step.type = StepType::StartMotor;

        bool ok = engine.executeStep(step);
        QVERIFY(ok);
        QVERIFY(startCalled);
    }

    // ------------------------------------------------------------------------
    // AC-4: executeStep StopMotor calls callback
    // ------------------------------------------------------------------------
    void testExecuteStepStopMotor()
    {
        AutomationEngine engine;

        bool stopCalled = false;
        engine.setMotorStopCallback([&]() {
            stopCalled = true;
            return true;
        });

        TestStep step;
        step.type = StepType::StopMotor;

        bool ok = engine.executeStep(step);
        QVERIFY(ok);
        QVERIFY(stopCalled);
    }

    // ------------------------------------------------------------------------
    // AC-4: executeStep Wait uses QTimer/QEventLoop
    // ------------------------------------------------------------------------
    void testExecuteStepWait()
    {
        AutomationEngine engine;

        TestStep step;
        step.type = StepType::Wait;
        step.params = {{"durationMs", "50"}};
        step.timeoutMs = 2000;

        QElapsedTimer timer;
        timer.start();

        bool ok = engine.executeStep(step);
        QVERIFY(ok);

        qint64 elapsed = timer.elapsed();
        QVERIFY2(elapsed >= 40, "Wait step should take at least the requested duration");
        QVERIFY2(elapsed < 500, "Wait step should not exceed timeout / take too long");
    }

    // ------------------------------------------------------------------------
    // AC-4: executeStep Assert checks min/max range on DataBus value
    // ------------------------------------------------------------------------
    void testExecuteStepAssertPass()
    {
        // Publish a value within range
        auto& reg = TopicRegistry::instance();
        TopicId speedId = reg.findTopic("Speed");
        QVERIFY(speedId > 0);
        DataBus::instance().publish(speedId, 1000.0f);

        AutomationEngine engine;

        TestStep step;
        step.type = StepType::Assert;
        step.params = {{"channel", "Speed"}, {"min", "500"}, {"max", "1500"}};

        bool ok = engine.executeStep(step);
        QVERIFY(ok);
    }

    void testExecuteStepAssertFailBelowMin()
    {
        auto& reg = TopicRegistry::instance();
        TopicId speedId = reg.findTopic("Speed");
        DataBus::instance().publish(speedId, 100.0f);  // below min of 500

        AutomationEngine engine;

        TestStep step;
        step.type = StepType::Assert;
        step.params = {{"channel", "Speed"}, {"min", "500"}, {"max", "1500"}};

        bool ok = engine.executeStep(step);
        QVERIFY(!ok);
    }

    void testExecuteStepAssertFailAboveMax()
    {
        auto& reg = TopicRegistry::instance();
        TopicId speedId = reg.findTopic("Speed");
        DataBus::instance().publish(speedId, 2000.0f);  // above max of 1500

        AutomationEngine engine;

        TestStep step;
        step.type = StepType::Assert;
        step.params = {{"channel", "Speed"}, {"min", "500"}, {"max", "1500"}};

        bool ok = engine.executeStep(step);
        QVERIFY(!ok);
    }

    void testExecuteStepAssertUnknownChannel()
    {
        AutomationEngine engine;

        TestStep step;
        step.type = StepType::Assert;
        step.params = {{"channel", "NonExistentChannel_XYZ"}};

        bool ok = engine.executeStep(step);
        QVERIFY(!ok);
    }

    // ------------------------------------------------------------------------
    // AC-3: run() iterates all steps, emits stepStarted/stepCompleted signals
    // ------------------------------------------------------------------------
    void testRunEmitsStepSignals()
    {
        AutomationEngine engine;

        // Register callbacks so steps don't fail
        int setParamCalls = 0;
        engine.setSetParamCallback([&](const std::string&, const std::string&) {
            setParamCalls++;
            return true;
        });

        int motorStartCalls = 0;
        engine.setMotorStartCallback([&]() {
            motorStartCalls++;
            return true;
        });

        int motorStopCalls = 0;
        engine.setMotorStopCallback([&]() {
            motorStopCalls++;
            return true;
        });

        // Set up DataBus value for assert step
        auto& reg = TopicRegistry::instance();
        TopicId speedId = reg.findTopic("Speed");
        DataBus::instance().publish(speedId, 1000.0f);

        // Load the sample test case
        bool loaded = engine.loadTestCase(sampleJsonPath.toStdString());
        QVERIFY(loaded);

        // Spy on signals
        QSignalSpy spyStarted(&engine, &AutomationEngine::testStarted);
        QSignalSpy spyCompleted(&engine, &AutomationEngine::testCompleted);
        QSignalSpy spyStepStarted(&engine, &AutomationEngine::stepStarted);
        QSignalSpy spyStepDone(&engine, &AutomationEngine::stepCompleted);
        QSignalSpy spyProgress(&engine, &AutomationEngine::progressUpdated);

        // Run synchronously on main thread
        engine.run();

        // Verify testStarted emitted exactly once
        QCOMPARE(spyStarted.count(), 1);
        std::string caseName = qvariant_cast<std::string>(spyStarted.at(0).at(0));
        QCOMPARE(QString::fromStdString(caseName), QString("Motor Basic Test"));

        // Verify testCompleted emitted exactly once
        QCOMPARE(spyCompleted.count(), 1);
        TestResult result = qvariant_cast<TestResult>(spyCompleted.at(0).at(0));
        QVERIFY(result.passed);

        // Verify step signals for all 5 steps
        QCOMPARE(spyStepStarted.count(), 5);
        QCOMPARE(spyStepDone.count(), 5);

        // Verify all steps succeeded
        for (int i = 0; i < 5; ++i) {
            QCOMPARE(spyStepDone.at(i).at(1).toBool(), true);
        }

        // Verify progress was emitted (at least step count + final)
        QVERIFY(spyProgress.count() >= 5);

        // Verify callbacks were called
        QCOMPARE(setParamCalls, 1);
        QCOMPARE(motorStartCalls, 1);
        QCOMPARE(motorStopCalls, 1);
    }

    // ------------------------------------------------------------------------
    // AC-3 (edge): run() with empty test case emits an immediate failure
    // ------------------------------------------------------------------------
    void testRunEmptyTestCase()
    {
        AutomationEngine engine;

        // Manually set an empty test case
        TestCase emptyTc;
        emptyTc.name = "Empty";
        engine.setCurrentTestCase(emptyTc);

        QSignalSpy spyCompleted(&engine, &AutomationEngine::testCompleted);

        engine.run();

        QCOMPARE(spyCompleted.count(), 1);
        TestResult result = qvariant_cast<TestResult>(spyCompleted.at(0).at(0));
        QVERIFY(!result.passed);
    }

    // ------------------------------------------------------------------------
    // AC-5: TestRunner::runAsync runs in background thread, emits runnerFinished
    // ------------------------------------------------------------------------
    void testTestRunnerAsync()
    {
        // Register callbacks via engine BEFORE moving to worker thread
        // (engine is on main thread during construction)
        AutomationEngine engine;
        engine.setSetParamCallback([&](const std::string&, const std::string&) { return true; });
        engine.setMotorStartCallback([&]() { return true; });
        engine.setMotorStopCallback([&]() { return true; });

        // Set up DataBus for assert
        auto& reg = TopicRegistry::instance();
        TopicId speedId = reg.findTopic("Speed");
        DataBus::instance().publish(speedId, 1000.0f);

        // Load test case
        bool loaded = engine.loadTestCase(sampleJsonPath.toStdString());
        QVERIFY(loaded);

        TestCase tc = engine.currentTestCase();

        // Create TestRunner — this moves the engine to a worker thread
        TestRunner runner(&engine);

        QSignalSpy spyFinished(&runner, &TestRunner::runnerFinished);

        // Run async
        runner.runAsync(tc);
        QVERIFY(runner.isRunning());

        // Wait for runnerFinished (max 5 seconds)
        bool received = spyFinished.wait(5000);
        QVERIFY2(received, "runnerFinished signal not received within 5 seconds");

        QCOMPARE(spyFinished.count(), 1);
        TestResult result = qvariant_cast<TestResult>(spyFinished.at(0).at(0));
        QVERIFY(result.passed);
        QCOMPARE(QString::fromStdString(result.caseName), QString("Motor Basic Test"));
    }

    // ------------------------------------------------------------------------
    // AC-5 (edge): runAsync with no engine set
    // ------------------------------------------------------------------------
    void testTestRunnerNoEngine()
    {
        // Create a runner with null engine
        TestRunner runner(nullptr);

        QSignalSpy spyFinished(&runner, &TestRunner::runnerFinished);

        TestCase dummy;
        runner.runAsync(dummy);

        bool received = spyFinished.wait(1000);
        QVERIFY(received);
        QCOMPARE(spyFinished.count(), 1);
        TestResult result = qvariant_cast<TestResult>(spyFinished.at(0).at(0));
        QVERIFY(!result.passed);
    }

    // ------------------------------------------------------------------------
    // Utility: custom step registration and execution
    // ------------------------------------------------------------------------
    void testCustomStepRegistration()
    {
        AutomationEngine engine;

        bool customCalled = false;
        engine.registerCustomStep("myHandler", [&](const TestStep&) {
            customCalled = true;
            return true;
        });

        TestStep step;
        step.type = StepType::Custom;
        step.params = {{"handler", "myHandler"}};

        bool ok = engine.executeStep(step);
        QVERIFY(ok);
        QVERIFY(customCalled);
    }

    void testReadParameterStep()
    {
        AutomationEngine engine;

        engine.setReadParamCallback([&](const std::string& name) -> std::string {
            if (name == "Voltage") return "220.5";
            return "";
        });

        TestStep step;
        step.type = StepType::ReadParameter;
        step.params = {{"name", "Voltage"}};

        bool ok = engine.executeStep(step);
        QVERIFY(ok);
    }

    void testStopOnFailure()
    {
        AutomationEngine engine;

        // Set up a test case where step 1 fails and stopOnFailure is true
        TestCase tc;
        tc.name = "StopOnFail";
        tc.stopOnFailure = true;

        // Step 0: passes
        {
            TestStep step;
            step.type = StepType::SetParameter;
            step.description = "Passing step";
            step.params = {{"Param", "1"}};
            tc.steps.push_back(step);
        }
        // Step 1: fails (no callback registered → fails)
        {
            TestStep step;
            step.type = StepType::StartMotor;
            step.description = "Failing step";
            tc.steps.push_back(step);
        }
        // Step 2: should NOT be executed
        {
            TestStep step;
            step.type = StepType::StopMotor;
            step.description = "Should be skipped";
            tc.steps.push_back(step);
        }

        // Only register SetParam callback
        engine.setSetParamCallback([&](const std::string&, const std::string&) { return true; });
        engine.setCurrentTestCase(tc);

        QSignalSpy spyStepStarted(&engine, &AutomationEngine::stepStarted);
        QSignalSpy spyStepDone(&engine, &AutomationEngine::stepCompleted);

        engine.run();

        // Only 2 steps should have started (0 and 1), step 2 should be skipped
        QCOMPARE(spyStepStarted.count(), 2);
        QCOMPARE(spyStepDone.count(), 2);
        QCOMPARE(spyStepDone.at(0).at(1).toBool(), true);   // step 0 passed
        QCOMPARE(spyStepDone.at(1).at(1).toBool(), false);  // step 1 failed
    }

    void testPauseResumeStop()
    {
        AutomationEngine engine;
        QCOMPARE(engine.isRunning(), false);
        QCOMPARE(engine.isPaused(), false);

        engine.pause();
        QCOMPARE(engine.isPaused(), true);

        engine.resume();
        QCOMPARE(engine.isPaused(), false);

        engine.stop();
        // stop() just sets stopRequested, not running/paused
    }
};

QTEST_MAIN(TestAutomationEngine)
#include "test_automation_engine.moc"
