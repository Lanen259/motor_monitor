# 编码规范 (Coding Standard)

## 目标

建立统一的 C++ 编码规范，确保 Motor Monitor 上位机项目代码风格一致、可读性强、可维护性高。规范覆盖命名、文件组织、错误处理、资源管理、并发编程等核心方面，并配套自动化检查工具。

---

## 设计原则

1. **一致性优先**：所有贡献者遵循同一套规则，降低认知负担。
2. **现代 C++**：使用 C++20 标准，充分利用现代特性（concepts, ranges, coroutines, modules 等）。
3. **Qt6 惯例**：遵循 Qt 框架的命名和设计惯例，与 Qt API 风格一致。
4. **安全第一**：RAII 管理所有资源，智能指针优先，杜绝裸指针所有权。
5. **自动化保障**：clang-format 统一格式，clang-tidy 静态分析，CI 强制检查。

---

## 类/模块关系

### 项目模块分层

```
┌──────────────────────────────────────────────────────────────┐
│                      UI Layer (ui/)                          │
│  MainWindow, Panels, Widgets, Dialogs, Themes                │
│  依赖: core, data, device                                    │
├──────────────────────────────────────────────────────────────┤
│                    Data Layer (data/)                         │
│  DataBus, DataModel, DataRecorder, DataExporter              │
│  依赖: core, logger                                          │
├──────────────────────────────────────────────────────────────┤
│                   Device Layer (device/)                      │
│  DeviceManager, MotorController, DeviceConfig                │
│  依赖: core, protocol, logger                                │
├──────────────────────────────────────────────────────────────┤
│                  Protocol Layer (protocol/)                   │
│  ProtocolBase, ModbusRTU, CANopen, ProtocolFactory           │
│  依赖: core, logger                                          │
├──────────────────────────────────────────────────────────────┤
│                   Core Layer (core/)                          │
│  App, ThreadPool, EventBus, PluginManager, ConfigManager     │
│  依赖: logger                                                │
├──────────────────────────────────────────────────────────────┤
│                  Logger Layer (logger/)                       │
│  LogManager, LogSink, LogEntry                               │
│  依赖: 无（底层基础设施）                                     │
└──────────────────────────────────────────────────────────────┘
```

### 文件组织规则

- 每个公开类一个 `.h` / `.cpp` 文件对，文件名与类名对应（snake_case）
- 内部实现细节放入 `detail/` 子目录或 `internal/` 命名空间
- 接口类（纯虚类）以 `I` 前缀命名，如 `IProtocol`, `IDevice`
- 跨模块依赖通过接口（纯虚类）解耦，而非直接依赖具体实现
- 工具函数和辅助类放入 `utils/` 目录，不依赖业务模块

### 依赖规则

| 规则 | 说明 |
|------|------|
| 单向依赖 | 上层可依赖下层，下层不可依赖上层 |
| 接口隔离 | 模块间通过纯虚接口通信，不直接引用实现类 |
| 循环禁止 | 任何两个模块间不允许存在循环依赖 |
| 头文件最小化 | 头文件仅包含必要的 `#include`，优先使用前向声明 |

---

## 数据流

### 编译期数据流：Include 顺序

```
源文件编译时，头文件按以下顺序引入：

1. 自身的头文件     →  #include "motor_controller.h"
2. Qt 头文件         →  #include <QObject>, <QTimer>
3. 标准库头文件      →  #include <memory>, <chrono>
4. 项目头文件        →  #include "device/device_config.h"

目的：确保自身头文件自包含（无隐式依赖），检测循环依赖。
```

### 运行时数据流：从协议到 UI

```
硬件设备
  │ 原始字节流
  ▼
Protocol Layer (protocol/)
  │ 解析 → 结构化数据 (MotorData)
  ▼
Device Layer (device/)
  │ 处理 → 设备状态 + 事件
  ▼
Data Layer (data/)
  │ DataBus 发布/订阅
  ▼
UI Layer (ui/)
  │ 渲染 → 波形、仪表盘、参数面板
  ▼
用户可见
```

### 错误传播路径

```
底层错误 (Protocol/Device)
  │ Result<T>::err()
  ▼
中间层 (Data/Core)
  │ 传播或转换
  ▼
上层 (UI)
  │ 转换为用户可读消息
  ▼
QMessageBox / StatusBar / LogPanel
```

---

## API 接口规划

### 公共 API 设计规则

| 规则 | 说明 |
|------|------|
| 明确所有权 | 通过参数类型表明所有权：`unique_ptr` = 转移，`const&` = 借用，`*` = 可选借用 |
| 返回值优先 | 优先使用返回值而非输出参数；多返回值使用 `std::tuple` 或 `struct` |
| Result 模式 | 可失败的操作返回 `Result<T>`，不可失败的返回 `T` 或 `void` |
| 默认参数 | 使用默认参数减少重载，但不超过 3 个默认参数 |
| 虚函数 | 公共接口使用 NVI 模式（非虚接口 + 私有虚函数） |
| 工厂方法 | 需要多态创建时使用工厂方法或工厂类，而非暴露构造函数 |

### 接口命名规范

```cpp
// 获取类接口
class MotorController {
public:
    // 动作: 动词开头
    Result<void> start();
    Result<void> stop();
    Result<void> configure(const MotorConfig& config);

    // 查询: get + 名词
    double getCurrentSpeed() const;
    MotorState getState() const;

    // 布尔查询: is/has/can + 名词
    bool isConnected() const;
    bool hasError() const;
    bool canExecute() const;

    // 设置: set + 名词
    void setSpeedLimit(double limit);
    void setEnabled(bool enabled);
};
```

### 回调与信号

```cpp
// 异步回调: 使用 std::function + Qt 信号
class MotorController : public QObject {
    Q_OBJECT

public:
    // 同步操作返回 Result
    Result<double> readRegister(uint16_t addr);

    // 异步操作通过信号通知
    void readRegisterAsync(uint16_t addr);

signals:
    void registerReadComplete(uint16_t addr, double value);
    void errorOccurred(ErrorCode code, const QString& message);
    void stateChanged(MotorState oldState, MotorState newState);
};
```

---

## 命名规范

### 类/结构体/枚举

| 元素 | 风格 | 示例 |
|------|------|------|
| 类名 | PascalCase | `MotorController`, `SerialPortManager` |
| 结构体名 | PascalCase | `LogEntry`, `DeviceConfig` |
| 枚举类型名 | PascalCase | `LogLevel`, `PluginType` |
| 枚举值 | PascalCase | `LogLevel::Debug`, `PluginType::Protocol` |
| 类型别名 | PascalCase | `using MotorId = uint32_t;` |
| 模板参数 | PascalCase 或单字母 | `template<typename T>` 或 `template<typename ElementType>` |

### 函数/方法

| 元素 | 风格 | 示例 |
|------|------|------|
| 公共方法 | camelCase | `startMotor()`, `getCurrentSpeed()` |
| 私有方法 | camelCase | `validateConfig()`, `parseResponse()` |
| 全局函数 | camelCase | `calculateCRC()`, `parseHexString()` |
| getter | `get` + PascalCase | `getMotorId()`, `getState()` |
| setter | `set` + PascalCase | `setSpeed()`, `setEnabled()` |
| 布尔 getter | `is`/`has`/`can` + PascalCase | `isConnected()`, `hasError()`, `canExecute()` |

### 变量

| 元素 | 风格 | 示例 |
|------|------|------|
| 局部变量 | camelCase | `motorSpeed`, `packetLength` |
| 成员变量 | camelCase + `m_` 前缀 | `m_motorId`, `m_isConnected` |
| 静态成员变量 | camelCase + `s_` 前缀 | `s_instance`, `s_maxRetries` |
| 全局变量 | camelCase + `g_` 前缀 | `g_appConfig`, `g_logManager` |
| 常量 | UPPER_CASE | `MAX_RETRY_COUNT`, `DEFAULT_TIMEOUT_MS` |

### 命名空间

```cpp
namespace MotorMonitor {           // 项目根命名空间
namespace Plugin {                 // 子命名空间
namespace Protocol {               // 更深层级
    class ModbusRTU { ... };
}
}
}

// 不使用 using namespace 在头文件中
// .cpp 文件中可使用 using namespace，但建议仅限函数作用域内
```

### 文件命名

```
类名: MotorController
  → motor_controller.h / motor_controller.cpp

类名: SerialPortManager
  → serial_port_manager.h / serial_port_manager.cpp

类名: IPluginHost
  → i_plugin_host.h  (接口类保留 I 前缀)
```

---

## 文件组织

### 头文件结构

```cpp
// motor_controller.h
#pragma once                              // 1. 头文件保护

#include <QObject>                        // 2. Qt 头文件
#include <QTimer>

#include <memory>                         // 3. 标准库头文件
#include <chrono>

#include "device/device_config.h"         // 4. 项目头文件
#include "protocol/protocol_base.h"

namespace MotorMonitor {

class MotorController : public QObject {
    Q_OBJECT

public:
    // 构造/析构
    explicit MotorController(const DeviceConfig& config, QObject* parent = nullptr);
    ~MotorController() override;

    // 禁用拷贝
    MotorController(const MotorController&) = delete;
    MotorController& operator=(const MotorController&) = delete;

    // 公共方法
    Result<void> start();
    Result<void> stop();
    double getCurrentSpeed() const;

    // Q_PROPERTY
    Q_PROPERTY(double speed READ getCurrentSpeed NOTIFY speedChanged)

signals:
    void speedChanged(double speed);
    void errorOccurred(const QString& message);

private:
    // 私有方法
    void onTimerTick();
    Result<void> sendCommand(const QByteArray& cmd);

    // 成员变量
    DeviceConfig m_config;
    double m_currentSpeed = 0.0;
    QTimer* m_timer = nullptr;
};

} // namespace MotorMonitor
```

### 源文件结构

```cpp
// motor_controller.cpp
#include "motor_controller.h"              // 1. 自己的头文件

#include <algorithm>                       // 2. 标准库头文件
#include <cmath>

#include <QSerialPort>                     // 3. Qt 头文件

#include "logger/logger.h"                 // 4. 项目头文件
#include "utils/crc16.h"

namespace MotorMonitor {

MotorController::MotorController(const DeviceConfig& config, QObject* parent)
    : QObject(parent)
    , m_config(config)
    , m_timer(new QTimer(this))
{
    connect(m_timer, &QTimer::timeout, this, &MotorController::onTimerTick);
}

// ...

} // namespace MotorMonitor
```

### 目录结构约定

```
src/
├── core/              # 核心框架（应用启动、线程模型、事件总线）
├── device/            # 设备抽象层
├── protocol/          # 通信协议实现
├── ui/                # 用户界面
│   ├── panels/        # 面板组件
│   ├── widgets/       # 自定义控件
│   └── dialogs/       # 对话框
├── data/              # 数据模型、数据总线
├── logger/            # 日志系统
├── plugin/            # 插件系统
├── automation/        # 自动化测试
├── utils/             # 工具类
└── main.cpp           # 入口
```

---

## 错误处理

### Result<T> 模式

```cpp
// 可恢复错误使用 Result<T>
template<typename T>
class Result {
public:
    static Result<T> ok(T value);
    static Result<T> err(ErrorCode code, std::string message);

    bool isOk() const;
    bool isErr() const;
    T& value();
    const T& value() const;
    const ErrorCode& errorCode() const;
    const std::string& errorMessage() const;

    // 链式处理
    template<typename F>
    auto map(F&& f) -> Result<decltype(f(std::declval<T>()))>;

    template<typename F>
    auto andThen(F&& f) -> decltype(f(std::declval<T>()));

private:
    // ...
};

// 使用示例
Result<MotorConfig> loadConfig(const std::string& path) {
    auto file = openFile(path);
    if (!file.isOk()) {
        return Result<MotorConfig>::err(
            ErrorCode::FileNotFound,
            "Cannot open config file: " + path
        );
    }
    auto config = parseConfig(file.value());
    if (!config.isOk()) {
        return config;  // 传播错误
    }
    return Result<MotorConfig>::ok(config.value());
}
```

### 异常使用

```cpp
// 异常仅用于编程错误（不可恢复）
void setMotorCount(int count) {
    if (count < 0 || count > MAX_MOTORS) {
        throw std::invalid_argument("Motor count must be between 0 and " +
                                    std::to_string(MAX_MOTORS));
    }
    m_motorCount = count;
}

// 前置条件检查
void processPacket(const QByteArray& data) {
    assert(!data.isEmpty() && "Packet must not be empty");
    // ...
}
```

### 错误处理原则

| 场景 | 处理方式 |
|------|----------|
| 文件不存在 | `Result<T>::err()` |
| 网络超时 | `Result<T>::err()` |
| 协议解析失败 | `Result<T>::err()` |
| 参数越界 | `std::invalid_argument` |
| 空指针解引用 | `assert()` + 崩溃 |
| 资源耗尽 | `std::bad_alloc` (或 `Result<T>::err()` 如果不致命) |

---

## 资源管理

### RAII 原则

```cpp
// 文件句柄
class FileHandle {
public:
    explicit FileHandle(const std::string& path);
    ~FileHandle();  // 自动关闭
    FileHandle(const FileHandle&) = delete;
    FileHandle& operator=(const FileHandle&) = delete;
    FileHandle(FileHandle&& other) noexcept;
    FileHandle& operator=(FileHandle&& other) noexcept;
private:
    FILE* m_file = nullptr;
};

// 互斥锁
{
    std::lock_guard<std::mutex> lock(m_mutex);  // 自动加锁/解锁
    // 临界区代码
}  // 自动释放

// Qt 对象树
auto* timer = new QTimer(this);  // this 析构时自动删除 timer
```

### 智能指针

```cpp
// unique_ptr 默认选择
std::unique_ptr<MotorController> controller = std::make_unique<MotorController>();

// 所有权转移
auto other = std::move(controller);

// shared_ptr 仅在共享所有权场景使用
std::shared_ptr<DeviceConfig> config = std::make_shared<DeviceConfig>();
// 观察者使用 weak_ptr
std::weak_ptr<DeviceConfig> configObserver = config;

// 禁止裸指针表示所有权（仅允许非拥有观察指针）
DeviceConfig* raw = config.get();  // OK: 非拥有
```

### const 正确性

```cpp
class MotorController {
public:
    // 不修改状态的成员函数声明为 const
    double getCurrentSpeed() const { return m_currentSpeed; }
    bool isConnected() const { return m_isConnected; }

    // 参数为只读引用
    Result<void> configure(const DeviceConfig& config);

    // 返回值不应被修改时返回 const 引用
    const DeviceConfig& getConfig() const { return m_config; }

private:
    double m_currentSpeed = 0.0;
    bool m_isConnected = false;
    DeviceConfig m_config;
};
```

### noexcept

```cpp
// 不会抛出异常的函数声明 noexcept
class MotorController {
public:
    ~MotorController() noexcept = default;  // 析构函数默认 noexcept
    MotorController(MotorController&& other) noexcept;  // 移动构造
    MotorController& operator=(MotorController&& other) noexcept;  // 移动赋值

    double getCurrentSpeed() const noexcept { return m_currentSpeed; }
    bool isConnected() const noexcept { return m_isConnected; }
};
```

---

## Qt 约定

### QObject 使用

```cpp
class MotorPanel : public QWidget {
    Q_OBJECT

public:
    // parent 总是最后一个参数，默认 nullptr
    explicit MotorPanel(QWidget* parent = nullptr);

    // Q_PROPERTY 声明
    Q_PROPERTY(int motorCount READ getMotorCount WRITE setMotorCount NOTIFY motorCountChanged)

    int getMotorCount() const;
    void setMotorCount(int count);

signals:
    // 信号命名：名词 + 变化动词
    // 参数：描述变化的内容
    void motorCountChanged(int newCount);
    void motorStarted(int motorId);
    void errorOccurred(const QString& message);

public slots:
    // 槽命名：动词开头
    void startAllMotors();
    void stopMotor(int motorId);

private slots:
    void onTimerTimeout();
    void onDataReceived(const QByteArray& data);

private:
    int m_motorCount = 0;
};
```

### 信号/槽连接

```cpp
// 新式语法（编译期检查）
connect(motor, &MotorController::speedChanged,
        panel, &MotorPanel::onSpeedChanged);

// Lambda 连接
connect(button, &QPushButton::clicked, this, [this]() {
    startMotor(m_currentMotorId);
});

// Qt::QueuedConnection 用于跨线程
connect(worker, &Worker::dataReady,
        this, &MainWindow::onDataReady,
        Qt::QueuedConnection);
```

---

## 自动化工具

### clang-format 配置 (.clang-format)

```yaml
BasedOnStyle: Google
Language: Cpp
Standard: c++20

IndentWidth: 4
TabWidth: 4
UseTab: Never

ColumnLimit: 120

AccessModifierOffset: -4
AllowShortFunctionsOnASingleLine: None
AllowShortIfStatementsOnASingleLine: false
AllowShortLoopsOnASingleLine: false

BreakBeforeBraces: Custom
BraceWrapping:
  AfterClass: true
  AfterControlStatement: Never
  AfterEnum: true
  AfterFunction: true
  AfterNamespace: true
  AfterStruct: true
  AfterUnion: true
  BeforeCatch: false
  BeforeElse: false

IncludeBlocks: Regroup
IncludeCategories:
  - Regex: '^<Q.*>'
    Priority: 1
  - Regex: '^<.*>'
    Priority: 2
  - Regex: '^".*"'
    Priority: 3

PointerAlignment: Left
SortIncludes: true
```

### clang-tidy 配置 (.clang-tidy)

```yaml
Checks: >
  -*,
  bugprone-*,
  -bugprone-easily-swappable-parameters,
  cppcoreguidelines-*,
  -cppcoreguidelines-avoid-magic-numbers,
  -cppcoreguidelines-pro-type-reinterpret-cast,
  -cppcoreguidelines-pro-bounds-array-to-pointer-decay,
  modernize-*,
  -modernize-use-trailing-return-type,
  performance-*,
  readability-*,
  -readability-identifier-length,
  -readability-magic-numbers,

CheckOptions:
  - key: readability-identifier-naming.ClassCase
    value: CamelCase
  - key: readability-identifier-naming.StructCase
    value: CamelCase
  - key: readability-identifier-naming.EnumCase
    value: CamelCase
  - key: readability-identifier-naming.FunctionCase
    value: camelBack
  - key: readability-identifier-naming.MethodCase
    value: camelBack
  - key: readability-identifier-naming.VariableCase
    value: camelBack
  - key: readability-identifier-naming.MemberCase
    value: camelBack
  - key: readability-identifier-naming.MemberPrefix
    value: 'm_'
  - key: readability-identifier-naming.StaticMemberPrefix
    value: 's_'
  - key: readability-identifier-naming.GlobalVariablePrefix
    value: 'g_'
  - key: readability-identifier-naming.ConstantCase
    value: UPPER_CASE
```

---

## 代码审查检查清单

- [ ] 命名符合规范（类 PascalCase，方法 camelCase，成员 m_ 前缀）
- [ ] 头文件使用 `#pragma once`
- [ ] include 顺序正确（自身 → Qt → std → 项目）
- [ ] 资源使用 RAII 管理，无裸指针所有权
- [ ] 合理使用 unique_ptr / shared_ptr
- [ ] 成员函数正确标记 const
- [ ] 移动构造/赋值标记 noexcept
- [ ] 错误处理：可恢复用 Result<T>，编程错误用异常
- [ ] QObject 派生类有 Q_OBJECT 宏
- [ ] 信号/槽使用新式语法
- [ ] 无 magic number，使用命名常量
- [ ] 复杂逻辑有注释说明
- [ ] 通过了 clang-format 和 clang-tidy 检查

---

## 后续实现注意事项

1. **Git 钩子**：配置 pre-commit 钩子自动运行 clang-format，确保提交的代码格式一致。

2. **CI 集成**：CI 流水线中运行 clang-tidy（WarningsAsErrors），阻止不符合规范的代码合并。

3. **IDE 集成**：提供 .clang-format 和 .clang-tidy 配置文件，确保 VS Code / CLion / Qt Creator 自动加载。

4. **模块化规范**：当项目增长到足够大时，考虑使用 C++20 modules 替代传统头文件，但需评估编译器支持情况。

5. **Qt 版本**：最低要求 Qt 6.5，使用 CMake 作为构建系统，不使用 qmake。

6. **文档注释**：公共 API 使用 Doxygen 风格注释（`///` 或 `/** */`），便于生成 API 文档。

7. **编译警告**：启用 `-Wall -Wextra -Wpedantic`，将警告视为错误（`-Werror`）。

8. **Sanitizers**：开发阶段启用 AddressSanitizer 和 UndefinedBehaviorSanitizer，定期运行 ThreadSanitizer。

9. **代码覆盖率**：单元测试覆盖率目标 > 80%，使用 `gcov`/`lcov` 或 `llvm-cov`。

10. **规范更新**：本规范每季度评审一次，根据团队反馈和 C++ 标准演进持续改进。