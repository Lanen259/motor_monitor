# ScriptEngine — 脚本引擎设计

> **文档版本**: v1.0  
> **父文档**: [SystemArchitecture.md](../architecture/SystemArchitecture.md)  
> **关联模块**: AutomationEngineService, ParamManagerService, DataBus, Logger  

---

## 目标

设计一个嵌入式脚本引擎，为 Motor Studio 提供灵活的自动化控制和扩展能力。Phase 1 基于 Lua（sol2/sol3），Phase 3 可选支持 Python。脚本引擎需提供安全沙箱、异步执行、结构化错误处理，并与自动化测试框架深度集成。

### 核心目标

1. **轻量嵌入**：Lua 解释器 < 500KB，启动时间 < 10ms，内存占用 < 2MB
2. **安全隔离**：执行超时、内存限制、API 白名单、可选进程隔离
3. **异步能力**：基于 Lua 协程的异步操作，非阻塞 sleep/wait
4. **C++ API 绑定**：通过 sol2/sol3 自动绑定，暴露 Motor Studio 核心 API
5. **可扩展**：模块化 API 设计，新模块零侵入注册
6. **自动化集成**：脚本可作为 Automation 框架的步骤执行

---

## 设计原则

| 原则 | 说明 |
|------|------|
| **最小权限** | 脚本默认只能访问显式注册的 API，不暴露系统调用 |
| **防御式设计** | 所有脚本调用有超时保护，异常不传播到宿主进程 |
| **渐进式复杂度** | 简单脚本一行搞定，复杂脚本支持模块化、协程、错误处理 |
| **宿主线程安全** | 脚本在独立线程执行，与 DataBus/UI 通过消息队列通信 |
| **可观测性** | 脚本执行日志、性能指标、错误堆栈全部可追踪 |

---

## 架构：ScriptHost → ScriptBinding → ScriptSandbox

### 整体架构图

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                         ScriptEngineService                                  │
│                                                                              │
│  ┌──────────────────────────────────────────────────────────────────────┐   │
│  │                      ScriptHost (生命周期管理)                         │   │
│  │                                                                        │   │
│  │  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐                 │   │
│  │  │ ScriptManager │  │ ModuleLoader │  │ ScriptPool   │                 │   │
│  │  │ (load/run/    │  │ (require/    │  │ (预创建 Lua   │                 │   │
│  │  │  stop/reload) │  │  import)     │  │  state 池)    │                 │   │
│  │  └──────┬───────┘  └──────┬───────┘  └──────┬───────┘                 │   │
│  │         │                 │                 │                           │   │
│  └─────────┼─────────────────┼─────────────────┼───────────────────────────┘   │
│            │                 │                 │                               │
│            ▼                 ▼                 ▼                               │
│  ┌──────────────────────────────────────────────────────────────────────┐   │
│  │                    ScriptBinding (C++ → Lua API 桥接)                  │   │
│  │                                                                        │   │
│  │  ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌─────────────┐ │   │
│  │  │  motor   │ │ databus  │ │  logger  │ │automation│ │   sleep     │ │   │
│  │  │  module  │ │  module  │ │  module  │ │  module  │ │   module    │ │   │
│  │  └────┬─────┘ └────┬─────┘ └────┬─────┘ └────┬─────┘ └──────┬──────┘ │   │
│  │       │            │            │            │              │         │   │
│  └───────┼────────────┼────────────┼────────────┼──────────────┼─────────┘   │
│          │            │            │            │              │             │
│          ▼            ▼            ▼            ▼              ▼             │
│  ┌──────────────────────────────────────────────────────────────────────┐   │
│  │                     ScriptSandbox (安全沙箱)                           │   │
│  │                                                                        │   │
│  │  ┌──────────────┐  ┌──────────────┐  ┌──────────────────────────────┐ │   │
│  │  │ Timeout      │  │ Memory       │  │ API Whitelist                │ │   │
│  │  │ Watchdog     │  │ Limit        │  │ (允许调用的 API 列表)         │ │   │
│  │  │ (execution)  │  │ (2MB/8MB)    │  │                              │ │   │
│  │  └──────────────┘  └──────────────┘  └──────────────────────────────┘ │   │
│  │                                                                        │   │
│  │  可选: ┌──────────────────────────────────────────────────────────┐   │   │
│  │        │ Process Isolation (单独进程执行脚本)                       │   │   │
│  │        │ - IPC 通信 (pipe/socket)                                  │   │   │
│  │        │ - 崩溃不影响宿主进程                                       │   │   │
│  │        └──────────────────────────────────────────────────────────┘   │   │
│  └──────────────────────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────────────────────┘
```

### 执行流程

```
ScriptHost::execute(script_path)
    │
    ├── 1. 从 ScriptPool 获取 Lua state
    │   └── 或创建新的 Lua state
    │
    ├── 2. 加载脚本文件
    │   ├── 解析 Lua 语法
    │   ├── 注入 API 绑定 (motor, databus, logger, automation, sleep)
    │   └── 执行 require 加载依赖模块
    │
    ├── 3. 设置 Sandbox 约束
    │   ├── 设置执行超时 (lua_sethook + 指令计数)
    │   ├── 设置内存限制 (lua_setallocf)
    │   └── 验证 API 白名单
    │
    ├── 4. 执行脚本 (在独立线程)
    │   ├── 调用脚本入口函数 (main 或全局作用域)
    │   ├── 协程调度 (异步操作)
    │   └── 捕获错误
    │
    ├── 5. 收集结果
    │   ├── 返回值
    │   ├── 日志输出
    │   ├── 执行时间
    │   └── 错误信息 (如果有)
    │
    └── 6. 归还 Lua state 到 ScriptPool
```

---

## 核心类/模块关系

### ScriptHost

```cpp
class ScriptHost {
public:
    // ====== 生命周期 ======
    void init(const ScriptConfig& config);
    void shutdown();
    
    // ====== 脚本管理 ======
    ScriptResult executeFile(const std::filesystem::path& path);
    ScriptResult executeString(const std::string& code, const std::string& name = "<inline>");
    ScriptResult executeFunction(const std::string& func_name, 
                                  const std::vector<ParamValue>& args);
    
    // ====== 控制 ======
    void stop(ScriptId id);
    void stopAll();
    bool isRunning(ScriptId id) const;
    std::vector<ScriptId> getRunningScripts() const;
    
    // ====== 模块管理 ======
    void registerModule(const std::string& name, std::unique_ptr<IScriptModule> module);
    void unregisterModule(const std::string& name);
    
    // ====== 脚本池 ======
    size_t poolSize() const;
    void setPoolSize(size_t size);
    
    // ====== 信号 ======
    Signal<ScriptId, ScriptState> onStateChanged;
    Signal<ScriptId, std::string> onScriptOutput;      // 脚本 print() 输出
    Signal<ScriptId, ScriptResult> onScriptCompleted;
    Signal<ScriptId, ScriptError> onScriptError;

private:
    std::unique_ptr<ScriptPool> pool_;
    std::unordered_map<std::string, std::unique_ptr<IScriptModule>> modules_;
    ScriptConfig config_;
};
```

### ScriptBinding

```cpp
class ScriptBinding {
public:
    // 将 C++ API 绑定到 Lua state
    void bindAll(sol::state& lua);
    
    // 注册单个模块
    void bindModule(sol::state& lua, const std::string& name, IScriptModule& module);
    
    // 解绑
    void unbindAll(sol::state& lua);

private:
    std::vector<std::unique_ptr<IScriptModule>> modules_;
};

// API 模块接口
class IScriptModule {
public:
    virtual ~IScriptModule() = default;
    virtual std::string name() const = 0;
    virtual std::string version() const = 0;
    virtual void bind(sol::state& lua) = 0;
    virtual void unbind(sol::state& lua) {}
};
```

### ScriptSandbox

```cpp
class ScriptSandbox {
public:
    struct Config {
        std::chrono::milliseconds execution_timeout{30'000};  // 30s
        size_t memory_limit_bytes{8 * 1024 * 1024};           // 8MB
        std::vector<std::string> api_whitelist;               // 空 = 全部允许
        bool enable_process_isolation{false};
        bool enable_file_io{false};                           // 是否允许文件 I/O
        std::filesystem::path allowed_path;                   // 允许访问的路径
    };
    
    explicit ScriptSandbox(const Config& config);
    
    // 设置沙箱约束
    void apply(sol::state& lua);
    void remove(sol::state& lua);
    
    // 检查 API 调用
    bool isApiAllowed(const std::string& api_name) const;
    
    // 超时检查
    void checkTimeout();

private:
    Config config_;
    std::chrono::steady_clock::time_point start_time_;
    
    // 指令计数 hook (Lua 5.4 支持)
    static void instructionHook(lua_State* L, lua_Debug* ar);
};
```

---

## 暴露的 API 模块

### 1. motor 模块

```lua
-- 参数读写
motor.read_param(param_id)           -- 读取参数值 → number/string/bool
motor.write_param(param_id, value)   -- 写入参数值
motor.get_param_info(param_id)       -- 获取参数描述 → table

-- 电机控制
motor.enable()                       -- 使能电机
motor.disable()                      -- 禁用电机
motor.emergency_stop()               -- 紧急停止
motor.set_speed(rpm)                 -- 设置目标转速
motor.set_acceleration(rpm_per_s)    -- 设置加速度
motor.ramp_speed(target, ramp_time, steps)  -- 斜坡调速

-- 状态查询
motor.get_speed()                    -- 获取当前转速
motor.get_current()                  -- 获取当前电流
motor.get_temperature()              -- 获取电机温度
motor.get_faults()                   -- 获取故障状态 → table
motor.clear_faults()                 -- 清除故障
motor.is_enabled()                   -- 电机是否使能
motor.is_running()                   -- 电机是否运行

-- 示例
local speed = motor.read_param(65536)
motor.ramp_speed(3000, 5000, 50)
motor.wait_for(function() return motor.get_speed() >= 2950 end, 10000)
```

### 2. databus 模块

```lua
-- 订阅数据
databus.subscribe(topic_id, callback)  -- 订阅 Topic
databus.unsubscribe(subscription_id)   -- 取消订阅

-- 读取数据
local dp = databus.get_latest(topic_id)  -- 获取最新值
local history = databus.get_history(topic_id, from_ms, to_ms)  -- 获取历史数据

-- 发送命令
databus.send_command(cmd_type, params)  -- 发送命令

-- 示例
local sub_id = databus.subscribe(0x0100, function(dp)
    logger.info("Speed: " .. dp.value .. " RPM")
end)
sleep.ms(5000)
databus.unsubscribe(sub_id)
```

### 3. logger 模块

```lua
logger.trace("message")
logger.debug("message")
logger.info("message")
logger.warn("message")
logger.error("message")
logger.fatal("message")

-- 结构化日志
logger.info("Motor started", {speed = 1000, current = 2.5})

-- 获取日志
local recent = logger.get_recent(100)  -- 最近 100 条日志
```

### 4. automation 模块

```lua
-- 测试控制
automation.run_test("tests/motor_ramp.json")  -- 执行 JSON 测试用例
automation.run_test_suite("suites/regression.json")

-- 断言
automation.assert_in_range(param_id, min, max, "message")
automation.assert_equals(actual, expected, "message")
automation.assert_true(condition, "message")

-- 数据记录
automation.start_recording({65536, 65537}, 100)  -- 100Hz 记录
automation.stop_recording()
automation.get_recorded_data()  -- 获取记录的数据

-- 步骤控制
automation.skip_step("reason")
automation.fail_step("reason")

-- 示例
automation.start_recording({65536, 65537, 65538}, 100)
motor.ramp_speed(3000, 5000, 50)
sleep.ms(1000)
automation.stop_recording()

local data = automation.get_recorded_data()
automation.assert_in_range(65536, 2900, 3100, "Speed should be 3000±100 RPM")
```

### 5. sleep 模块（协程异步）

```lua
-- 非阻塞 sleep (yield 协程)
sleep.ms(1000)     -- 睡眠 1000 毫秒
sleep.seconds(2)   -- 睡眠 2 秒
sleep.until(condition_func, timeout_ms)  -- 等待条件满足

-- 协程管理
sleep.yield()      -- 主动让出 CPU

-- 示例: 非阻塞等待
motor.ramp_speed(3000, 5000, 50)
sleep.until(function()
    return motor.get_speed() >= 2950
end, 10000)
logger.info("Speed reached target!")
```

---

## 协程异步机制

### 设计原理

```
传统阻塞 sleep:
  motor.set_speed(1000)
  sleep(2000)          ← 阻塞线程 2 秒，浪费资源
  motor.set_speed(2000)

协程异步 sleep:
  motor.set_speed(1000)
  sleep.ms(2000)       ← yield 协程，释放线程给其他脚本
  motor.set_speed(2000)
```

### 实现

```cpp
class AsyncSleepModule : public IScriptModule {
public:
    void bind(sol::state& lua) override {
        // 创建 sleep 表
        auto sleep_table = lua.create_table();
        
        // sleep.ms(ms) — 协程 yield
        sleep_table.set_function("ms", [this](sol::this_state s, int ms) {
            lua_State* L = s;
            
            // 设置唤醒时间
            auto coro_id = getCoroutineId(L);
            wakeup_times_[coro_id] = Clock::now() + std::chrono::milliseconds(ms);
            
            // yield 协程
            return lua_yield(L, 0);
        });
        
        // sleep.until(condition_func, timeout_ms)
        sleep_table.set_function("until", [this](sol::this_state s, 
                                                   sol::function condition, 
                                                   int timeout_ms) {
            lua_State* L = s;
            auto deadline = Clock::now() + std::chrono::milliseconds(timeout_ms);
            
            while (Clock::now() < deadline) {
                // 检查条件
                auto result = condition();
                if (result.valid() && result.get<bool>()) {
                    return;  // 条件满足
                }
                
                // yield 10ms
                auto coro_id = getCoroutineId(L);
                wakeup_times_[coro_id] = Clock::now() + std::chrono::milliseconds(10);
                lua_yield(L, 0);
            }
            
            // 超时
            luaL_error(L, "sleep.until() timed out after %d ms", timeout_ms);
        });
        
        lua["sleep"] = sleep_table;
    }
    
    // 协程调度器: 检查哪些协程可以恢复
    void scheduleCoroutines(sol::state& lua) {
        auto now = Clock::now();
        for (auto& [coro_id, wake_time] : wakeup_times_) {
            if (now >= wake_time) {
                // 恢复协程
                // ...
            }
        }
    }

private:
    std::unordered_map<int, std::chrono::steady_clock::time_point> wakeup_times_;
};
```

### 协程调度器

```cpp
class CoroutineScheduler {
public:
    // 添加协程到调度队列
    void addCoroutine(lua_State* L, int coro_ref);
    void removeCoroutine(int coro_id);
    
    // 主调度循环 (在 ScriptHost 线程中运行)
    void schedule(sol::state& lua);
    
    // 检查是否有活跃协程
    bool hasActiveCoroutines() const;
    
    // 停止所有协程
    void stopAll();

private:
    struct CoroutineInfo {
        lua_State* L;
        int ref;
        std::chrono::steady_clock::time_point wake_time;
        CoroutineState state;  // Running, Suspended, Dead
    };
    std::unordered_map<int, CoroutineInfo> coroutines_;
};
```

---

## 安全机制

### 1. 执行超时

```cpp
// 使用 Lua hook 实现指令计数超时
void ScriptSandbox::applyTimeout(sol::state& lua, std::chrono::milliseconds timeout) {
    // 估算: 假设 10M 指令/秒，超时 30s = 300M 指令
    constexpr int INSTRUCTIONS_PER_MS = 10'000;
    int max_instructions = static_cast<int>(timeout.count() * INSTRUCTIONS_PER_MS);
    
    lua_sethook(lua.lua_state(), instructionTimeoutHook, LUA_MASKCOUNT, max_instructions);
}

static void instructionTimeoutHook(lua_State* L, lua_Debug* ar) {
    luaL_error(L, "Script execution timed out");
}
```

### 2. 内存限制

```cpp
// 自定义内存分配器
static void* restrictedAlloc(void* ud, void* ptr, size_t osize, size_t nsize) {
    auto* sandbox = static_cast<ScriptSandbox*>(ud);
    
    if (nsize > 0) {
        size_t current = sandbox->currentMemoryUsage();
        size_t new_total = current - osize + nsize;
        
        if (new_total > sandbox->memoryLimit()) {
            return nullptr;  // 超出内存限制
        }
        sandbox->setCurrentMemoryUsage(new_total);
    }
    
    if (nsize == 0) {
        free(ptr);
        sandbox->setCurrentMemoryUsage(sandbox->currentMemoryUsage() - osize);
        return nullptr;
    }
    
    return realloc(ptr, nsize);
}

// 在创建 Lua state 时设置
lua_setallocf(L, restrictedAlloc, sandbox_ptr);
```

### 3. API 白名单

```cpp
class ApiWhitelist {
public:
    void allow(const std::string& module, const std::string& function);
    void deny(const std::string& module, const std::string& function);
    void allowAll();
    void denyAll();
    
    bool isAllowed(const std::string& module, const std::string& function) const;

private:
    // 格式: {"motor.write_param": true, "motor.read_param": true, ...}
    std::unordered_map<std::string, bool> whitelist_;
    bool default_allow_{true};  // 空列表 = 全部允许
};

// 在 API 绑定层检查
template<typename F>
auto wrapWithWhitelist(const std::string& module, const std::string& func, F&& f) {
    return [whitelist = &whitelist_, module, func, f = std::forward<F>(f)](auto&&... args) {
        if (!whitelist->isAllowed(module, func)) {
            throw std::runtime_error("API call denied: " + module + "." + func);
        }
        return f(std::forward<decltype(args)>(args)...);
    };
}
```

### 4. 进程隔离（可选）

```cpp
class ProcessIsolationSandbox {
public:
    // 在独立进程中执行脚本
    ScriptResult executeInProcess(const std::filesystem::path& script_path);
    
    // 进程崩溃恢复
    bool isProcessAlive() const;
    void restartProcess();

private:
    // IPC 通信: stdin/stdout 管道
    std::unique_ptr<Subprocess> script_process_;
    
    // 序列化协议: JSON-RPC over pipe
    JsonRpcChannel channel_;
};
```

---

## 错误处理

### 结构化错误报告

```cpp
struct ScriptError {
    enum class Type {
        SyntaxError,       // Lua 语法错误
        RuntimeError,      // 运行时错误
        TimeoutError,      // 执行超时
        MemoryError,       // 内存超限
        ApiError,          // API 调用错误
        SandboxError,      // 沙箱违规
        InternalError,     // 引擎内部错误
    };
    
    Type type;
    std::string message;
    std::string source_file;
    int line_number;
    std::string function_name;
    std::string stack_trace;
    std::chrono::system_clock::time_point timestamp;
};

struct ScriptResult {
    bool success;
    std::optional<ParamValue> return_value;
    std::vector<ScriptError> errors;
    std::vector<std::string> log_output;
    std::chrono::microseconds execution_time;
    size_t memory_used_bytes;
    uint32_t instructions_executed;
};
```

### Lua 错误捕获

```lua
-- 脚本内的错误处理
local ok, result = pcall(function()
    motor.write_param(65536, 999999)  -- 可能失败
end)

if not ok then
    logger.error("Write failed: " .. result)
end

-- 使用 xpcall 获取堆栈
local function risky_operation()
    motor.ramp_speed(3000, 5000, 50)
end

local function error_handler(err)
    logger.error("Script error: " .. debug.traceback(err))
end

xpcall(risky_operation, error_handler)
```

---

## 脚本模块系统

### require/import

```lua
-- 用户脚本: main.lua
local utils = require("utils.helpers")       -- 加载 utils/helpers.lua
local motor_test = require("tests.common")   -- 加载 tests/common.lua

-- 使用模块
utils.wait_for_speed(3000, 10000)
motor_test.run_startup_sequence()
```

### 模块搜索路径

```lua
-- 默认搜索路径 (在 ScriptHost 中配置)
-- scripts/?.lua
-- scripts/?/init.lua
-- scripts/modules/?.lua
-- scripts/tests/?.lua
```

### 模块缓存

```cpp
class ModuleCache {
public:
    // 加载并缓存模块
    sol::object require(sol::state& lua, const std::string& module_name);
    
    // 清除缓存 (热重载)
    void clear(const std::string& module_name = "");
    void clearAll();
    
    // 预加载关键模块
    void preload(const std::vector<std::string>& modules);

private:
    std::unordered_map<std::string, sol::object> cache_;
    std::filesystem::path search_path_;
};
```

---

## 与 Automation 框架集成

### 脚本作为测试步骤

```json
{
  "type": "ExecuteScript",
  "id": "script_step_1",
  "description": "执行自定义脚本验证",
  "script": "tests/scripts/custom_check.lua",
  "params": {
    "target_speed": 3000,
    "tolerance": 100
  },
  "timeout_ms": 30000,
  "sandbox": {
    "memory_limit_mb": 8,
    "allow_file_io": false
  }
}
```

### 脚本调用测试用例

```lua
-- 在脚本中执行 JSON 测试用例
local result = automation.run_test("tests/motor_ramp.json")
if result.passed then
    logger.info("Ramp test passed!")
else
    logger.error("Ramp test failed: " .. result.error)
end
```

### 脚本与自动化数据共享

```lua
-- 脚本写入 TestContext 变量
automation.set_variable("custom_threshold", 85.0)

-- 后续 JSON 步骤可以引用
-- { "type": "AssertCondition", "expr": "MotorTemperature < ${custom_threshold}" }
```

---

## API 接口规划

### ScriptEngineService 公共接口

```cpp
class ScriptEngineService {
public:
    // ====== 生命周期 ======
    void init(const ScriptConfig& config);
    void shutdown();
    
    // ====== 脚本执行 ======
    ScriptResult executeFile(const std::filesystem::path& path);
    ScriptResult executeString(const std::string& code, 
                                const std::string& name = "<inline>");
    ScriptResult executeFunction(const std::string& func_name,
                                  const std::vector<ParamValue>& args);
    
    // ====== 控制 ======
    void stop(ScriptId id);
    void stopAll();
    ScriptState getState(ScriptId id) const;
    std::vector<ScriptInfo> getRunningScripts() const;
    
    // ====== 模块管理 ======
    void registerApiModule(std::unique_ptr<IScriptModule> module);
    void registerLuaModule(const std::filesystem::path& path);
    void reloadModule(const std::string& name);
    
    // ====== 沙箱配置 ======
    void setSandboxConfig(ScriptId id, const ScriptSandbox::Config& config);
    ScriptSandbox::Config getSandboxConfig(ScriptId id) const;
    
    // ====== 脚本池 ======
    size_t getPoolSize() const;
    void setPoolSize(size_t size);
    void prewarmPool();  // 预创建 Lua state
    
    // ====== 信号 ======
    Signal<ScriptId, ScriptState> onStateChanged;
    Signal<ScriptId, std::string> onScriptOutput;
    Signal<ScriptId, ScriptResult> onScriptCompleted;
    Signal<ScriptId, ScriptError> onScriptError;
    
    // ====== 统计 ======
    ScriptStats getStats() const;
};

enum class ScriptState {
    Idle, Loading, Running, Suspended, Stopping, Completed, Error, Timeout
};

struct ScriptConfig {
    std::filesystem::path scripts_directory{"scripts/"};
    std::filesystem::path modules_directory{"scripts/modules/"};
    size_t pool_size{4};                          // Lua state 池大小
    std::chrono::milliseconds default_timeout{30'000};
    size_t default_memory_limit_mb{8};
    bool enable_process_isolation{false};
    bool enable_file_io{true};
    std::vector<std::string> api_whitelist;       // 空 = 全部允许
    std::vector<std::string> search_paths;        // require 搜索路径
};

struct ScriptStats {
    uint64_t total_executions;
    uint64_t total_errors;
    uint64_t total_timeouts;
    std::chrono::microseconds avg_execution_time;
    size_t current_pool_size;
    size_t active_scripts;
};
```

---

## Phase 3: Python 支持（未来）

### 设计概要

```
┌──────────────────────────────────────────────────────┐
│                 ScriptEngineService                   │
│                                                       │
│  ┌──────────────────┐    ┌──────────────────────────┐│
│  │  Lua Backend      │    │  Python Backend (Phase 3) ││
│  │  - sol2/sol3      │    │  - pybind11              ││
│  │  - 轻量, 快速     │    │  - 丰富生态              ││
│  │  - 协程原生支持   │    │  - asyncio 异步          ││
│  └──────────────────┘    └──────────────────────────┘│
│                                                       │
│  统一接口: IScriptBackend                             │
│  - execute(script, context) → ScriptResult            │
│  - bindModule(name, module)                           │
│  - stop(id)                                           │
└──────────────────────────────────────────────────────┘
```

```cpp
class IScriptBackend {
public:
    virtual ~IScriptBackend() = default;
    virtual ScriptResult execute(const std::string& code, ScriptContext& ctx) = 0;
    virtual void stop() = 0;
    virtual void bindModule(std::unique_ptr<IScriptModule> module) = 0;
    virtual std::string language() const = 0;  // "lua", "python"
};

class LuaBackend : public IScriptBackend { /* ... */ };
class PythonBackend : public IScriptBackend { /* ... */ };  // Phase 3
```

---

## 后续实现注意事项

1. **sol2 vs sol3 选择**：sol2 成熟稳定，支持 Lua 5.1-5.4；sol3 是 sol2 的现代 C++ 重写，API 更简洁，但成熟度较低。建议 Phase 1 使用 sol2（已验证），迁移 sol3 风险较低时再切换。

2. **Lua 版本选择**：推荐 Lua 5.4，支持 `lua_sethook` 指令计数模式（`LUA_MASKCOUNT`），是实现超时控制的基础。LuaJIT 性能更好但不支持指令计数 hook，需要替代超时方案。

3. **ScriptPool 设计**：预创建 Lua state 池，避免每次执行脚本时重新创建 state（开销约 5-10ms）。每个 state 预加载 API 绑定，脚本执行完后重置全局表（`lua_settop(L, 0)`）但保留 API 绑定。

4. **协程调度器与主循环**：协程调度器在 `ScriptHost` 线程中运行，以 100Hz 频率检查可唤醒的协程。调度器使用 `std::chrono::steady_clock` 保证时间精度，不受系统时间调整影响。

5. **sleep.ms() 精度**：基于协程 yield/resume 的 sleep 精度受调度频率限制（100Hz = 10ms 粒度）。对于 < 10ms 的 sleep，使用 `std::this_thread::sleep_for` 在协程线程中阻塞（不 yield）。

6. **API 线程安全**：Lua state 不是线程安全的，所有 API 调用必须在 ScriptHost 线程中执行。`motor.write_param()` 等操作通过消息队列异步发送到 DataBus 线程，协程 yield 等待响应。

7. **错误恢复**：脚本执行中的 Lua 错误（`lua_error`）应被 `sol::protected_function` 捕获，转换为 `ScriptError` 结构体，不传播到宿主进程。`pcall` 包装所有脚本入口。

8. **脚本热重载**：`ModuleCache` 支持运行时清除缓存，`ScriptHost::reloadModule()` 清除指定模块缓存，下次 `require` 时重新加载。适用于开发调试阶段。

9. **print() 重定向**：脚本中的 `print()` 调用重定向到 `Signal<ScriptId, std::string> onScriptOutput`，在 UI 日志面板中显示。使用 `lua_setprintfunc` 或重写 `print` 全局函数。

10. **Python 后端的进程隔离**：Python 后端天然适合进程隔离（`pybind11` + 独立 Python 解释器进程）。通过 JSON-RPC over Unix socket / named pipe 通信，Python 进程崩溃由 `ScriptHost` 检测并重启。

11. **脚本调试支持**：未来 Phase 2 可集成 Lua Debugger（如 MobDebug），支持断点、单步执行、变量查看。通过 `lua_sethook` 的 `LUA_MASKLINE` 模式实现逐行执行。

12. **脚本版本管理**：脚本文件纳入 Git 版本控制。`ScriptResult` 中记录脚本文件的 SHA256 哈希，确保测试报告可追溯到确切的脚本版本。