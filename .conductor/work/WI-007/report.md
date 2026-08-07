status: done
files_changed: [AutomationEngine.h, AutomationEngine.cpp, TestRunner.cpp, tests/unit/automation/sample_test.json, tests/unit/automation/test_automation_engine.cpp, tests/CMakeLists.txt]
evidence:
  - what_was_done: |
      AutomationEngine was already substantially implemented. Key fixes:
      1. Added setCurrentTestCase() — thread-safe setter for TestRunner integration
      2. Fixed TestRunner::runAsync() to call setCurrentTestCase() (was Q_UNUSED)
      3. Fixed TestRunner destructor to move engine back to main thread before quit
      4. Added Q_DECLARE_METATYPE + qRegisterMetaType for QSignalSpy compatibility
      5. 20 test methods covering all StepTypes, JSON loading, signal emission, async execution
      6. sample_test.json with all step types for integration testing
risks:
  - Unit test uses QTest::qFindTestData; requires running from build dir
  - std::string in signals needs MOC compatibility (verified for Qt 5.14)
decisions:
  - problem: TestRunner::runAsync() ignored its testCase parameter (Q_UNUSED)
    chosen: Added setCurrentTestCase() to engine, called before queuing run()
    reason: API contract now honest; caller can pass TestCase directly
  - problem: Engine thread-affinity mismatch on TestRunner destruction
    chosen: moveToThread back in destructor before quit
    reason: Prevents Qt warnings without changing ownership model
