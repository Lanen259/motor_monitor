WI-ID: WI-007
类型: feature
负责人: worker
依赖: [P0-02]
范围:
  修改: [AutomationEngine.h, AutomationEngine.cpp, TestRunner.h, TestRunner.cpp]
  新增: [tests/unit/automation/test_automation_engine.cpp, tests/unit/automation/sample_test.json]
验收标准:
  - AutomationEngine::loadTestCase(jsonFilePath) 解析 JSON 文件并返回 TestCase 对象
  - JSON 用例格式包含 name, description, steps 数组，每个 step 含 type, params, timeoutMs
  - AutomationEngine::run() 遍历所有步骤并发射 stepStarted/stepCompleted 信号
  - AutomationEngine::executeStep() 根据 StepType 执行不同逻辑
  - TestRunner::runAsync(testCase) 在后台线程执行，完成后发射 runnerFinished 信号
  - 单元测试 test_automation_engine.cpp (20 test methods) 覆盖所有步骤类型
非目标: [不涉及流程图 UI（P3-02）、不涉及报告生成（P3-03）]
状态: done
