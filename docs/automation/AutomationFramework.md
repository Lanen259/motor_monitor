# AutomationFramework — 自动化测试框架设计

> **文档版本**: v1.0  
> **父文档**: [SystemArchitecture.md](../architecture/SystemArchitecture.md)  
> **关联模块**: AutomationEngineService, ParamManagerService, DataBus, ScriptEngine  

---

## 目标

设计一个工业级自动化测试框架，为 Motor Studio 提供可编排、可复现、可报告的电机驱动测试能力。框架支持 JSON 定义测试用例，通过插件化步骤扩展测试场景，生成结构化的测试报告，并具备事务回滚和超时保护机制。

### 核心目标

1. **声明式测试**：测试用例通过 JSON 定义，不依赖代码编译
2. **可组合**：测试步骤可自由组合，支持循环、分支、子测试嵌套
3. **可扩展**：StepFactory 插件化，新步骤类型零侵入注册
4. **事务安全**：操作失败时回滚已执行的副作用
5. **可追溯**：完整的测试报告，包含每步执行细节、时序数据、断言结果
6. **工业可靠**：超时看门狗、紧急停止、并行执行（未来）

---

## 设计原则

| 原则 | 说明 |
|------|------|
| **关注点分离** | 测试定义（JSON）与测试执行（C++）分离；报告生成独立于执行引擎 |
| **单一职责** | 每个 ITestStep 只做一件事；TestContext 只管理状态，不执行步骤 |
| **开闭原则** | 通过 StepFactory 注册扩展新步骤，不修改框架核心代码 |
| **失败隔离** | 单个步骤失败不影响其他测试用例；子测试失败可在父测试中捕获 |
| **显式超时** | 每个步骤有独立超时，框架级 Watchdog 兜底 |
| **不可变测试定义** | TestDefinition 加载后不可变，执行时拷贝到 TestContext |

---

## 架构：TestDefinition → TestRunner → TestContext → TestReport

### 整体架构图

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                          AutomationEngineService                             │
│                                                                              │
│  ┌──────────────────┐    ┌──────────────────┐    ┌──────────────────────┐   │
│  │ TestLoader        │    │ TestRunner        │    │ ReportGenerator      │   │
│  │ (JSON/YAML →      │    │ (execution engine) │    │ (PDF/HTML/JSON)      │   │
│  │  TestDefinition)  │    │                    │    │                      │   │
│  └────────┬─────────┘    └────────┬───────────┘    └──────────┬───────────┘   │
│           │                       │                            │               │
│           ▼                       ▼                            │               │
│  ┌──────────────────┐    ┌──────────────────┐                  │               │
│  │ TestDefinition    │    │ TestContext       │                  │               │
│  │ - name            │    │ - variables       │                  │               │
│  │ - steps[]         │    │ - transaction log │                  │               │
│  │ - config          │    │ - step_results[]  │                  │               │
│  │ - assertions[]    │    │ - timeout_watchdog│                  │               │
│  └──────────────────┘    └────────┬──────────┘                  │               │
│                                   │                              │               │
│                                   │ executes                     │               │
│                                   ▼                              │               │
│  ┌──────────────────────────────────────────────────────────┐    │               │
│  │                    StepFactory                             │    │               │
│  │  ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌──────────────┐ │    │               │
│  │  │SetParam  │ │ WaitFor  │ │RecordData│ │AssertCondition│ │    │               │
│  │  └──────────┘ └──────────┘ └──────────┘ └──────────────┘ │    │               │
│  │  ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌──────────────┐ │    │               │
│  │  │RampSpeed │ │SendCommand│ │  Delay   │ │  Loop/Branch │ │    │               │
│  │  └──────────┘ └──────────┘ └──────────┘ └──────────────┘ │    │               │
│  │  ┌──────────┐ ┌──────────┐                                │    │               │
│  │  │ SubTest  │ │  Plugin  │  ← 外部插件                     │    │               │
│  │  └──────────┘ └──────────┘                                │    │               │
│  └──────────────────────────────────────────────────────────┘    │               │
│                                   │                              │               │
│                                   ▼                              ▼               │
│                          ┌──────────────────────────────────────────────┐     │
│                          │              TestReport                       │     │
│                          │  - summary: pass/fail/timeout/error           │     │
│                          │  - step_details[]                             │     │
│                          │  - recorded_data[]                            │     │
│                          │  - timeline (execution trace)                 │     │
│                          └──────────────────────────────────────────────┘     │
└─────────────────────────────────────────────────────────────────────────────┘
```

### 执行流程

```
TestLoader::load(json_path)
    │
    ▼
TestDefinition (immutable)
    │
    ▼
TestRunner::execute(TestDefinition, TestContext)
    │
    ├── 1. 初始化 TestContext
    │   ├── 注入依赖 (ParamManager, DataBus, Logger)
    │   ├── 初始化变量作用域
    │   └── 启动 Watchdog 定时器
    │
    ├── 2. 遍历 steps[]
    │   └── for each step:
    │       ├── StepFactory::create(step.type) → ITestStep
    │       ├── step.execute(context)
    │       │   ├── 成功 → 记录结果, 更新 context
    │       │   ├── 失败 + continueOnFailure=true → 记录失败, 继续
    │       │   └── 失败 + continueOnFailure=false → 回滚, 中止
    │       └── 记录步骤执行时间
    │
    ├── 3. 执行全局断言
    │   └── for each assertion:
    │       └── assertion.evaluate(context) → pass/fail
    │
    ├── 4. 生成 TestReport
    │   └── ReportGenerator::generate(context, TestDefinition)
    │
    └── 5. 清理
        ├── 停止 Watchdog
        └── 提交/回滚事务
```

---

## 核心接口

### ITestStep 接口

```cpp
class ITestStep {
public:
    virtual ~ITestStep() = default;
    
    // ====== 步骤标识 ======
    virtual std::string id() const = 0;          // 步骤实例 ID (JSON 中定义)
    virtual std::string type() const = 0;        // 步骤类型: "SetParameter", "WaitFor" 等
    virtual std::string description() const { return ""; }
    
    // ====== 执行 ======
    virtual StepResult execute(TestContext& ctx) = 0;
    
    // ====== 回滚 (事务支持) ======
    virtual StepResult undo(TestContext& ctx) { 
        return StepResult::ok();  // 默认无操作
    }
    
    // ====== 配置 ======
    virtual std::chrono::milliseconds timeout() const { return 30'000ms; }
    virtual uint32_t maxRetries() const { return 0; }
    virtual bool continueOnFailure() const { return false; }
    
    // ====== 验证 ======
    virtual StepValidationResult validate(const TestContext& ctx) const {
        return StepValidationResult::ok();
    }
};

// 步骤执行结果
struct StepResult {
    enum class Status { Success, Failure, Timeout, Skipped, Error };
    
    Status status;
    std::string message;
    std::chrono::microseconds elapsed;
    std::optional<ParamValue> output_value;  // 步骤产出值
    std::unordered_map<std::string, std::string> metadata;
    
    static StepResult ok(std::string msg = "");
    static StepResult fail(std::string reason);
    static StepResult timeout(std::string reason);
    static StepResult skipped(std::string reason);
    
    bool isSuccess() const { return status == Status::Success; }
};
```

### IAssertion 接口 (策略模式)

```cpp
class IAssertion {
public:
    virtual ~IAssertion() = default;
    
    virtual std::string type() const = 0;
    virtual AssertionResult evaluate(const TestContext& ctx) = 0;
    virtual std::string description() const = 0;
};

struct AssertionResult {
    bool passed;
    std::string message;
    std::optional<double> expected_value;
    std::optional<double> actual_value;
    std::chrono::microseconds evaluation_time;
};

// 内置断言类型
class ValueInRangeAssertion : public IAssertion { /* ... */ };
class ValueEqualsAssertion : public IAssertion { /* ... */ };
class ExpressionAssertion : public IAssertion { /* ... */ };
class TimeSeriesAssertion : public IAssertion { /* ... */ };
class NoFaultAssertion : public IAssertion { /* ... */ };
```

### StepFactory (插件化)

```cpp
class StepFactory {
public:
    static StepFactory& instance();
    
    using StepCreator = std::function<std::unique_ptr<ITestStep>(const StepConfig&)>;
    
    // 注册步骤类型 (线程安全)
    bool registerType(const std::string& type_name, StepCreator creator);
    void unregisterType(const std::string& type_name);
    
    // 创建步骤实例
    std::unique_ptr<ITestStep> create(const StepConfig& config);
    
    // 查询
    std::vector<std::string> registeredTypes() const;
    bool hasType(const std::string& type_name) const;
    
    // 线程安全注册
    // 使用 std::shared_mutex 保护内部 registry

private:
    mutable std::shared_mutex mutex_;
    std::unordered_map<std::string, StepCreator> registry_;
};
```

### TestContext

```cpp
class TestContext {
public:
    // ====== 依赖注入 ======
    std::shared_ptr<ParamManagerService> paramManager;
    std::shared_ptr<IDataBus> dataBus;
    std::shared_ptr<Logger> logger;
    
    // ====== 变量管理 ======
    // 设置变量 (支持嵌套作用域)
    void setVariable(const std::string& name, ParamValue value);
    ParamValue getVariable(const std::string& name) const;
    bool hasVariable(const std::string& name) const;
    
    // 作用域管理
    void pushScope(const std::string& scope_name);
    void popScope();
    std::vector<std::string> getVariableNames() const;
    
    // ====== 事务支持 ======
    void beginTransaction();
    void commitTransaction();
    void rollbackTransaction();
    
    // 记录副作用 (用于回滚)
    void recordSideEffect(std::function<void(TestContext&)> undo_action);
    
    // ====== 步骤状态 ======
    void addStepResult(const StepResult& result);
    const std::vector<StepResult>& getStepResults() const;
    
    // ====== 数据记录 ======
    void recordDataPoint(const std::string& label, const DataPoint& dp);
    void recordMetric(const std::string& name, double value, const std::string& unit);
    const std::vector<RecordedData>& getRecordedData() const;
    
    // ====== 执行控制 ======
    void requestStop();              // 请求停止 (紧急停止)
    bool isStopRequested() const;
    void setProgress(double percent); // 0.0 - 1.0
    
    // ====== 测试元数据 ======
    std::string testName;
    std::string testId;
    std::chrono::system_clock::time_point startTime;
    std::chrono::system_clock::time_point endTime;
    std::unordered_map<std::string, std::string> tags;

private:
    std::vector<std::unordered_map<std::string, ParamValue>> variable_scopes_;
    std::vector<std::function<void(TestContext&)>> transaction_undo_stack_;
    std::vector<StepResult> step_results_;
    std::vector<RecordedData> recorded_data_;
    std::atomic<bool> stop_requested_{false};
};
```

---

## 内置步骤类型

### 1. SetParameter — 设置参数

```cpp
class SetParameterStep : public ITestStep {
public:
    // 配置: { "paramId": 65536, "value": 3000 }
    // 或:    { "paramId": 65536, "value": "${target_speed}" }
    StepResult execute(TestContext& ctx) override;
    StepResult undo(TestContext& ctx) override {
        // 恢复旧值
        ctx.paramManager->writeParameter(param_id_, old_value_);
        return StepResult::ok();
    }
private:
    ParamId param_id_;
    ParamValue target_value_;
    ParamValue old_value_;  // 执行前保存
};
```

### 2. WaitFor — 等待条件

```cpp
class WaitForStep : public ITestStep {
public:
    // 配置: { "condition": "MotorSpeed >= 2900", "timeout_ms": 5000, "poll_interval_ms": 100 }
    StepResult execute(TestContext& ctx) override;
};
```

### 3. RecordData — 记录数据

```cpp
class RecordDataStep : public ITestStep {
public:
    // 配置: { "paramIds": [65536, 65537], "duration_ms": 2000, "sample_rate_hz": 100 }
    StepResult execute(TestContext& ctx) override;
    // 在指定时长内以指定频率采样并记录到 TestContext
};
```

### 4. AssertCondition — 断言条件

```cpp
class AssertConditionStep : public ITestStep {
public:
    // 配置: { "type": "value_in_range", "paramId": 65536, "min": 2900, "max": 3100 }
    // 配置: { "type": "expression", "expr": "MotorSpeed >= 0 && MotorCurrent < MaxCurrent" }
    StepResult execute(TestContext& ctx) override;
};
```

### 5. RampSpeed — 斜坡调速

```cpp
class RampSpeedStep : public ITestStep {
public:
    // 配置: { "targetSpeed": 3000, "rampTime": 5000, "steps": 50 }
    // 在 5 秒内分 50 步将转速从当前值平滑升至 3000
    StepResult execute(TestContext& ctx) override;
    StepResult undo(TestContext& ctx) override;
};
```

### 6. SendCommand — 发送命令

```cpp
class SendCommandStep : public ITestStep {
public:
    // 配置: { "command": "EnableMotor", "args": {} }
    // 配置: { "command": "EmergencyStop", "args": {} }
    StepResult execute(TestContext& ctx) override;
};
```

### 7. Delay — 延时

```cpp
class DelayStep : public ITestStep {
public:
    // 配置: { "duration_ms": 2000 }
    StepResult execute(TestContext& ctx) override;
};
```

### 8. Loop — 循环

```cpp
class LoopStep : public ITestStep {
public:
    // 配置: { "count": 10, "steps": [...] }
    // 配置: { "while": "MotorSpeed < 3000", "max_iterations": 100, "steps": [...] }
    StepResult execute(TestContext& ctx) override;
};
```

### 9. Branch — 条件分支

```cpp
class BranchStep : public ITestStep {
public:
    // 配置: { "condition": "MotorTemperature > 80", "then": [...], "else": [...] }
    StepResult execute(TestContext& ctx) override;
};
```

### 10. SubTest — 子测试

```cpp
class SubTestStep : public ITestStep {
public:
    // 配置: { "testRef": "common/startup_check.json", "params": { "timeout": 5000 } }
    StepResult execute(TestContext& ctx) override;
};
```

---

## TestReport 生成

### TestReport 结构

```cpp
struct TestReport {
    // 概览
    std::string test_name;
    std::string test_id;
    std::chrono::system_clock::time_point start_time;
    std::chrono::system_clock::time_point end_time;
    std::chrono::milliseconds total_duration;
    
    TestResult summary;  // Pass / Fail / Timeout / Error / Aborted
    
    // 详细结果
    std::vector<StepReport> step_reports;
    std::vector<AssertionReport> assertion_reports;
    
    // 数据
    std::vector<RecordedDataSeries> data_series;
    
    // 统计
    uint32_t total_steps;
    uint32_t passed_steps;
    uint32_t failed_steps;
    uint32_t skipped_steps;
    double pass_rate;
    
    // 环境信息
    std::string device_model;
    std::string firmware_version;
    std::string software_version;
    std::unordered_map<std::string, std::string> environment;
};

struct TestResult {
    enum Outcome { Pass, Fail, Timeout, Error, Aborted };
    Outcome outcome;
    std::string message;
};

struct StepReport {
    std::string step_id;
    std::string step_type;
    std::string description;
    StepResult::Status status;
    std::string message;
    std::chrono::microseconds elapsed;
    std::chrono::system_clock::time_point timestamp;
    std::optional<std::string> error_detail;
};

struct RecordedDataSeries {
    std::string label;
    std::string unit;
    std::vector<std::pair<double, double>> time_series;  // (timestamp_ms, value)
};
```

### 报告生成器

```cpp
class ReportGenerator {
public:
    // 生成详细报告
    static std::string toJson(const TestReport& report, bool pretty = true);
    static std::string toHtml(const TestReport& report);
    
    // 生成 PDF (需要外部库: libharu / wkhtmltopdf)
    static std::vector<uint8_t> toPdf(const TestReport& report);
    
    // 生成摘要
    static std::string toSummaryText(const TestReport& report);
    
    // 报告比较
    static std::string diffReports(const TestReport& a, const TestReport& b);
};
```

---

## TestRunner 实现

```cpp
class TestRunner {
public:
    TestRunner();
    
    // 执行单个测试用例
    TestReport execute(const TestDefinition& def);
    
    // 执行测试套件
    std::vector<TestReport> executeSuite(const TestSuiteDefinition& suite);
    
    // 并行执行 (未来 Phase 2)
    std::vector<TestReport> executeParallel(const std::vector<TestDefinition>& defs, 
                                              size_t max_concurrency = 4);
    
    // 控制
    void stop();           // 停止当前测试
    void stopAll();        // 停止所有测试
    void pause();
    void resume();
    
    // 回调
    Signal<std::string, StepResult> onStepCompleted;
    Signal<double> onProgress;             // 0.0 - 1.0
    Signal<TestReport> onTestCompleted;
    Signal<std::string> onError;

private:
    // 执行单个步骤 (含重试逻辑)
    StepResult executeStep(ITestStep& step, TestContext& ctx);
    
    // 步骤回滚
    void rollbackSteps(std::vector<ITestStep*>& executed_steps, TestContext& ctx);
    
    // Watchdog
    void startWatchdog(TestContext& ctx, std::chrono::milliseconds timeout);
    void stopWatchdog();
    
    std::atomic<bool> stop_requested_{false};
    std::atomic<bool> paused_{false};
};
```

---

## 测试超时 Watchdog

```cpp
class TestWatchdog {
public:
    explicit TestWatchdog(std::chrono::milliseconds global_timeout = 300'000ms);  // 5 分钟
    
    void start();
    void stop();
    void reset();
    
    void setStepTimeout(std::chrono::milliseconds timeout);
    std::chrono::milliseconds remaining() const;
    
    bool hasExpired() const;
    
    // 回调: 超时时触发
    Signal<> onTimeout;

private:
    std::chrono::milliseconds global_timeout_;
    std::chrono::milliseconds step_timeout_;
    std::chrono::steady_clock::time_point start_time_;
    std::chrono::steady_clock::time_point step_start_time_;
    std::atomic<bool> running_{false};
    std::thread watchdog_thread_;
};
```

---

## 并行测试执行（未来 Phase 2）

```
┌──────────────────────────────────────────────────────────────────┐
│                    Parallel Test Executor                         │
│                                                                   │
│  TestSuite                                                        │
│  ├── Test A ──► Worker Thread 1 ──► TestReport A                 │
│  ├── Test B ──► Worker Thread 2 ──► TestReport B                 │
│  ├── Test C ──► Worker Thread 3 ──► TestReport C                 │
│  └── Test D ──► Worker Thread 4 ──► TestReport D                 │
│                                                                   │
│  约束:                                                             │
│  - 每个 Test 有独立的 TestContext (数据隔离)                       │
│  - 共享资源 (ParamManager) 通过参数锁协调                          │
│  - 同一 MCU 的测试串行化 (参数互斥)                                │
│  - 不同 MCU 的测试可完全并行                                       │
└──────────────────────────────────────────────────────────────────┘
```

---

## API 接口规划

### AutomationEngineService 公共接口

```cpp
class AutomationEngineService {
public:
    // ====== 生命周期 ======
    void init(std::shared_ptr<ParamManagerService> param,
              std::shared_ptr<IDataBus> databus);
    void shutdown();
    
    // ====== 测试管理 ======
    // 加载测试定义
    Result<TestDefinition> loadTest(const std::filesystem::path& json_path);
    Result<TestSuiteDefinition> loadSuite(const std::filesystem::path& json_path);
    
    // 执行测试
    TestReport executeTest(const TestDefinition& def);
    TestReport executeTest(const std::filesystem::path& json_path);
    std::vector<TestReport> executeSuite(const TestSuiteDefinition& suite);
    std::vector<TestReport> executeSuite(const std::filesystem::path& json_path);
    
    // 控制
    void stop();
    void stopAll();
    void pause();
    void resume();
    
    // 查询
    TestState currentState() const;
    std::vector<TestReport> getHistory(size_t limit = 50) const;
    
    // 报告
    void exportReport(const TestReport& report, 
                      const std::filesystem::path& output_path,
                      ReportFormat format = ReportFormat::HTML);
    
    // 信号
    Signal<std::string, StepResult> onStepCompleted;
    Signal<double> onProgress;
    Signal<TestReport> onTestCompleted;
    Signal<TestState> onStateChanged;
    Signal<std::string> onError;
};

enum class TestState {
    Idle, Loading, Running, Paused, Stopping, Completed, Error
};

enum class ReportFormat {
    JSON, HTML, PDF
};
```

---

## 后续实现注意事项

1. **步骤执行顺序保证**：`TestRunner::execute()` 严格按 JSON 中 `steps[]` 数组顺序执行。Loop 和 Branch 步骤内部步骤也按序执行。所有步骤的 `execute()` 调用在 WorkerPool 线程中同步执行，不跨线程。

2. **事务回滚链**：`TestContext` 维护一个 `undo_stack`，每个步骤执行成功后将其 `undo` 动作入栈。失败时从栈顶依次调用 `undo()`。注意：`undo()` 本身可能失败，需要记录但不中断回滚链。

3. **变量作用域**：`TestContext` 使用栈式作用域。`pushScope()` 在进入 Loop/Branch/SubTest 时调用，`popScope()` 在退出时调用。变量查找从当前作用域向上搜索父作用域。

4. **表达式引擎**：`WaitFor` 条件、`Branch` 条件、`AssertCondition` 表达式使用统一的表达式引擎。建议使用 `exprtk` (纯 C++ 头文件)，支持数学运算、逻辑运算、变量引用。表达式语法参考 `TestCaseDesign.md`。

5. **StepFactory 线程安全**：使用 `std::shared_mutex`，注册时 `unique_lock`，创建时 `shared_lock`。内置步骤在静态初始化阶段注册（`static RegisterBuiltinSteps`），确保 main() 之前就绪。

6. **TestReport 序列化**：JSON 格式使用 `nlohmann/json` 序列化，`RecordedData` 可能较大（>10MB），建议使用二进制格式（MessagePack）或仅存储文件路径引用。

7. **Watchdog 实现**：独立线程，每秒检查一次。超时后触发 `onTimeout` 回调，`TestRunner` 收到后调用 `stop()`。Watchdog 线程不执行任何业务逻辑，仅做时间检查。

8. **紧急停止语义**：`stop()` 是异步请求，设置 `stop_requested_` 标志。当前步骤完成后检查标志，若为 true 则跳过后续步骤。紧急停止不等同于回滚——已执行的步骤不自动回滚，由调用者决定是否回滚。

9. **SubTest 参数传递**：子测试通过 `params` 字段传递参数，在子测试中作为变量 `$param.xxx` 访问。子测试有独立的变量作用域，不能访问父测试变量（除非显式传递）。

10. **测试定义版本管理**：JSON 测试定义文件包含 `format_version` 字段，`TestLoader` 根据版本选择对应的解析器。支持向前兼容（新版本加载旧格式），拒绝向后兼容（旧版本加载新格式）。

11. **数据记录性能**：`RecordData` 步骤可能以 1kHz 采样率记录数据，持续数秒。使用预分配 vector + 无锁写入，避免在采样循环中分配内存。

12. **报告模板化**：HTML 报告使用模板引擎（如 `inja`），支持自定义报告模板。默认模板包含执行摘要、步骤时间线、数据图表（嵌入 ECharts/Chart.js）。