# 插件架构设计 (Plugin Architecture)

## 目标

构建一个可扩展、安全、高性能的插件系统，使 Motor Monitor 上位机能够通过插件动态加载新功能，而无需重新编译主程序。插件系统需支持多种插件类型，覆盖协议适配、设备驱动、测试步骤、UI面板、数据过滤、导出格式及自定义控件等场景。

---

## 设计原则

1. **C ABI 边界**：插件与宿主之间使用稳定的 C ABI 接口，避免 C++ ABI 兼容性问题（名称修饰、STL 版本差异等）。
2. **最小依赖**：插件仅依赖 `IPluginHost` 接口头文件，不链接主程序库。
3. **热加载支持**：支持运行时加载、卸载插件，无需重启主程序。
4. **安全第一**：数字签名验证 + 可选沙箱隔离 + 权限声明机制。
5. **版本兼容**：通过 `minHostVersion` 和 `getPluginAPIVersion()` 确保兼容性。
6. **启动性能**：插件缓存机制避免每次启动都重新扫描和验证。

---

## 类/模块关系

```
┌─────────────────────────────────────────────────────────────────────┐
│                        PluginManager (Singleton)                     │
│  - scan(searchPaths)                                                 │
│  - load(pluginId) / unload(pluginId)                                │
│  - getPlugin<T>(pluginId)                                            │
│  - getPluginsByType(type)                                            │
│  - activate(pluginId) / deactivate(pluginId)                        │
│  - shutdown()                                                        │
└──────────────┬──────────────────────────────────────────────────────┘
               │ owns
               ▼
┌──────────────────────────────┐    ┌──────────────────────────────┐
│        PluginRegistry        │    │       PluginCache            │
│  - register(manifest, entry) │    │  - cacheFilePath             │
│  - unregister(pluginId)      │    │  - loadCache()               │
│  - findByType(type)          │    │  - saveCache()               │
│  - findByDep(depId)          │    │  - invalidate(pluginId)      │
└──────────────────────────────┘    └──────────────────────────────┘
               │
               │ manages
               ▼
┌──────────────────────────────────────────────────────────────────────┐
│                        PluginInstance                                │
│  - manifest: PluginManifest                                          │
│  - handle: void*  (dlopen handle)                                    │
│  - entryPoint: IPlugin*                                              │
│  - state: PluginState { Unloaded, Loaded, Initialized, Active,       │
│                         Inactive, Error }                            │
│  - permissions: PermissionSet                                        │
│  - signature: SignatureInfo                                          │
└──────────────────────────────────────────────────────────────────────┘
               │
               │ implements
               ▼
┌──────────────────────────────────────────────────────────────────────┐
│                    IPlugin (C ABI Interface)                         │
│  + createPlugin(host: IPluginHost*) → IPlugin*                      │
│  + destroyPlugin(plugin: IPlugin*) → void                           │
│  + getPluginAPIVersion() → uint32_t                                 │
│                                                                      │
│  IPlugin vtable:                                                     │
│  + getManifest() → const PluginManifest*                            │
│  + initialize() → Result<void>                                      │
│  + shutdown() → void                                                 │
│  + getInterface(type: PluginType) → void*                           │
└──────────────────────────────────────────────────────────────────────┘
               │
               │ callback
               ▼
┌──────────────────────────────────────────────────────────────────────┐
│                    IPluginHost (C ABI Interface)                     │
│  + log(level, category, message) → void                             │
│  + getConfig(key) → const char*                                     │
│  + registerMenu(action, callback) → void                            │
│  + getDeviceManager() → IDeviceManager*                             │
│  + getDataBus() → IDataBus*                                         │
│  + requestPermission(perm) → bool                                   │
│  + getHostVersion() → Version                                       │
└──────────────────────────────────────────────────────────────────────┘
```

### 插件类型枚举

```cpp
enum class PluginType : uint32_t {
    Protocol      = 0x01,   // 通信协议实现 (Modbus, CANopen, etc.)
    Device        = 0x02,   // 设备驱动 (特定电机型号)
    TestStep      = 0x03,   // 自动化测试步骤
    UIPanel       = 0x04,   // UI 面板扩展
    DataFilter    = 0x05,   // 数据过滤器/处理器
    ExportFormat  = 0x06,   // 导出格式 (CSV, JSON, MATLAB, etc.)
    WidgetFactory = 0x07,   // 自定义控件工厂
};
```

### 插件状态机

```
Unloaded ──load()──▶ Loaded ──initialize()──▶ Initialized ──activate()──▶ Active
                        │                        │                          │
                        │                        │                          │
                        ▼                        ▼                          ▼
                      Error   ◀── onError ──   Error          deactivate()
                                                                           │
                        ◀─────────────── unload() ◀─────── Inactive ◀──────┘
```

---

## 数据流

### 插件加载流程

```
1. 启动/刷新
   │
2. PluginManager::scan()
   │  遍历 searchPaths 下所有 plugin.json
   ▼
3. 解析 Manifest → 验证字段完整性
   │
4. 签名验证（可选）
   │  校验 plugin.so 的数字签名
   ▼
5. 依赖检查
   │  检查所有 dependencies 是否已安装/可解析
   │  检查 minHostVersion 是否满足
   ▼
6. 拓扑排序
   │  基于依赖关系排序所有待加载插件
   │  检测循环依赖 → 报错
   ▼
7. 按序加载
   │  dlopen(plugin.so) → 查找 createPlugin / destroyPlugin / getPluginAPIVersion
   │  检查 API 版本兼容性
   ▼
8. 创建实例
   │  createPlugin(host) → IPlugin*
   ▼
9. 初始化
   │  plugin->initialize()
   │  请求权限 → 用户确认
   ▼
10. 注册到 Registry
    │  PluginRegistry::register(manifest, entryPoint)
    ▼
11. 激活
    │  plugin->activate()
    ▼
12. 就绪
```

### 卸载流程

```
1. deactivate() → 插件停止工作
2. 反注册：从 Registry 移除
3. shutdown() → 插件清理资源
4. destroyPlugin() → 释放 IPlugin 实例
5. dlclose() → 卸载动态库
6. 从缓存中移除
```

### 权限请求流程

```
Plugin::initialize()
  │
  ├─ requestPermission(Permission::FileSystem)
  │     │
  │     ▼
  │  IPluginHost::requestPermission()
  │     │
  │     ▼
  │  PermissionManager::check(pluginId, permission)
  │     │
  │     ├─ 已授权 → 返回 true
  │     │
  │     └─ 未授权 → 弹出权限确认对话框
  │           │
  │           ├─ 用户同意 → 记录授权 → 返回 true
  │           │
  │           └─ 用户拒绝 → 返回 false → 插件降级或报错
```

---

## Manifest 格式 (plugin.json)

```json
{
  "id": "com.example.protocol.modbus-rtu",
  "name": "Modbus RTU Protocol",
  "version": "1.2.0",
  "type": "Protocol",
  "entryPoint": "libmodbus_rtu_plugin",
  "minHostVersion": "1.0.0",
  "description": "Modbus RTU 协议实现",
  "author": "Example Corp",
  "license": "MIT",
  "dependencies": [
    {
      "id": "com.example.serial.base",
      "version": ">=1.0.0"
    }
  ],
  "permissions": [
    "SerialPort",
    "FileSystem:Read",
    "Network:LocalOnly"
  ],
  "tags": ["modbus", "rtu", "serial"],
  "homepage": "https://example.com/plugins/modbus-rtu",
  "icon": "icon.png"
}
```

### 字段说明

| 字段 | 类型 | 必需 | 说明 |
|------|------|------|------|
| `id` | string | ✅ | 全局唯一标识，反向域名格式 |
| `name` | string | ✅ | 人类可读名称 |
| `version` | string | ✅ | 语义化版本 |
| `type` | string | ✅ | 插件类型，见 `PluginType` 枚举 |
| `entryPoint` | string | ✅ | 动态库文件名（不含扩展名） |
| `minHostVersion` | string | ✅ | 最低宿主版本要求 |
| `description` | string | ❌ | 功能描述 |
| `author` | string | ❌ | 作者/组织 |
| `license` | string | ❌ | 许可证 |
| `dependencies` | array | ❌ | 依赖的其他插件 |
| `permissions` | array | ❌ | 需要的权限声明 |
| `tags` | array | ❌ | 搜索标签 |
| `homepage` | string | ❌ | 项目主页 |
| `icon` | string | ❌ | 图标文件路径 |

### 权限定义

```cpp
enum class Permission : uint32_t {
    SerialPort       = 1 << 0,   // 串口访问
    Network          = 1 << 1,   // 网络访问
    NetworkLocalOnly = 1 << 2,   // 仅本地网络
    FileSystemRead   = 1 << 3,   // 文件读取
    FileSystemWrite  = 1 << 4,   // 文件写入
    DeviceConfig     = 1 << 5,   // 设备配置修改
    UI               = 1 << 6,   // 创建 UI 元素
    DataBus          = 1 << 7,   // 数据总线读写
    Automation       = 1 << 8,   // 自动化测试执行
    System           = 1 << 9,   // 系统级操作 (高危)
};
```

---

## C ABI 边界

所有跨动态库边界的接口必须使用纯 C 调用约定：

```cpp
// plugin_api.h — 由宿主和插件共同引用

#ifdef __cplusplus
extern "C" {
#endif

// 导出函数（每个插件必须实现）
IPlugin* createPlugin(IPluginHost* host);
void     destroyPlugin(IPlugin* plugin);
uint32_t getPluginAPIVersion(void);

// 当前 API 版本
#define PLUGIN_API_VERSION 1

#ifdef __cplusplus
}
#endif

// IPlugin — 纯虚接口（C++ 内部使用，但 vtable 布局稳定）
class IPlugin {
public:
    virtual ~IPlugin() = default;
    virtual const PluginManifest* getManifest() const = 0;
    virtual Result<void> initialize() = 0;
    virtual void shutdown() = 0;
    virtual void* getInterface(PluginType type) = 0;
};

// IPluginHost — 宿主服务接口
class IPluginHost {
public:
    virtual ~IPluginHost() = default;
    virtual void log(LogLevel level, const char* category, const char* message) = 0;
    virtual const char* getConfig(const char* key) = 0;
    virtual bool requestPermission(uint32_t permission) = 0;
    virtual Version getHostVersion() const = 0;
    virtual IDeviceManager* getDeviceManager() = 0;
    virtual IDataBus* getDataBus() = 0;
};
```

### 关键约束

- 不允许跨边界传递 C++ STL 对象（`std::string`, `std::vector` 等）
- 字符串使用 `const char*`，所有权归调用方或使用明确的内存管理
- 接口通过纯虚类（vtable），不依赖 RTTI
- 模块边界内可自由使用 C++20，但导出接口必须扁平化为 C 类型

---

## 插件缓存机制

为加速启动，插件系统在首次扫描后生成缓存文件 `plugin_cache.json`：

```json
{
  "hostVersion": "1.2.0",
  "lastScan": "2026-08-05T08:00:00Z",
  "plugins": [
    {
      "id": "com.example.protocol.modbus-rtu",
      "version": "1.2.0",
      "path": "/plugins/modbus_rtu/plugin.json",
      "libraryPath": "/plugins/modbus_rtu/libmodbus_rtu_plugin.so",
      "sha256": "a1b2c3...",
      "signature": "valid",
      "lastVerified": "2026-08-05T08:00:00Z",
      "dependencies": ["com.example.serial.base"],
      "loadOrder": 1
    }
  ]
}
```

### 缓存策略

- 启动时先读缓存，仅对缓存中 `sha256` 变化的插件重新验证
- 新增插件通过目录扫描发现并追加到缓存
- 缓存失效条件：hostVersion 变化、插件文件修改、签名过期
- 可通过 `--clear-plugin-cache` 命令行参数强制重建

---

## 循环依赖检测

基于拓扑排序算法（Kahn 算法）：

```
1. 构建依赖图 G = (V, E)，V = 插件集合，E = 依赖关系
2. 计算每个节点的入度
3. 将入度为 0 的节点加入队列
4. 循环：取出节点 → 加入排序结果 → 其邻居入度-1 → 入度为0则入队
5. 若排序结果数量 < |V|，则存在循环依赖
6. 对循环依赖报错，列出参与循环的插件 ID
```

---

## 安全机制

### 1. 数字签名验证

```
签名流程：
  插件开发者 → 私钥签名(plugin.so + plugin.json) → 签名文件 plugin.sig
  宿主加载时 → 公钥验证(plugin.so, plugin.json, plugin.sig)
  验证失败 → 拒绝加载，记录安全日志
```

### 2. 沙箱隔离（可选）

- Linux: 使用 `seccomp-bpf` 限制系统调用
- Windows: 使用 `Job Objects` + 受限令牌
- macOS: 使用 `sandbox_init(3)` 配置沙箱
- 沙箱策略在 `plugin.json` 中声明，与 `permissions` 字段对应

### 3. 权限声明

- 插件在 manifest 中声明所需权限
- 宿主在首次加载时弹出权限确认对话框
- 用户可随时在设置中吊销已授权权限
- 权限检查在 `IPluginHost` 层强制执行，插件无法绕过

---

## API 接口规划

### PluginManager 公共接口

```cpp
class PluginManager {
public:
    static PluginManager& instance();

    // 扫描与发现
    void addSearchPath(const std::string& path);
    ScanResult scan();  // 扫描所有路径，返回发现的插件列表

    // 生命周期
    Result<void> load(const std::string& pluginId);
    Result<void> unload(const std::string& pluginId);
    Result<void> activate(const std::string& pluginId);
    Result<void> deactivate(const std::string& pluginId);
    void shutdownAll();

    // 查询
    bool isLoaded(const std::string& pluginId) const;
    PluginState getState(const std::string& pluginId) const;
    std::vector<PluginInfo> getPluginsByType(PluginType type) const;
    std::vector<PluginInfo> getAllPlugins() const;

    // 类型安全获取
    template<typename T>
    T* getPluginInterface(const std::string& pluginId);

    // 缓存
    void saveCache();
    void clearCache();

    // 事件
    Signal<const PluginInfo&> onPluginLoaded;
    Signal<const PluginInfo&> onPluginUnloaded;
    Signal<const PluginInfo&, const std::string&> onPluginError;
};
```

### 插件端实现模板

```cpp
// 每个插件提供这三个导出函数
extern "C" IPlugin* createPlugin(IPluginHost* host) {
    return new MyProtocolPlugin(host);
}

extern "C" void destroyPlugin(IPlugin* plugin) {
    delete plugin;
}

extern "C" uint32_t getPluginAPIVersion() {
    return PLUGIN_API_VERSION;
}
```

---

## 后续实现注意事项

1. **跨平台动态库加载**：`dlopen`/`dlclose`/`dlsym` (Linux/macOS)，`LoadLibrary`/`FreeLibrary`/`GetProcAddress` (Windows)。封装为 `DynamicLibrary` 类统一接口。

2. **Qt 事件循环兼容**：插件如需使用 Qt 信号槽，必须在插件初始化时传入 QApplication 指针，且插件内部创建的 QObject 应使用宿主线程的事件循环。

3. **内存管理**：插件分配的内存由插件释放，宿主分配的内存由宿主释放。跨边界传递的字符串使用 `strdup` 或明确的生命周期约定。

4. **插件热更新**：先 deactivate → unload → 替换文件 → load → activate。需确保旧版本的所有引用已释放。

5. **调试支持**：插件日志统一通过 `IPluginHost::log()` 发送，确保日志格式一致。插件 crash 不应导致宿主崩溃（信号处理或独立进程隔离）。

6. **版本兼容矩阵**：定义 `PLUGIN_API_VERSION` 与宿主版本的兼容矩阵，不兼容时拒绝加载并给出明确提示。

7. **插件市场**：远期可设计插件分发服务器，支持在线浏览、安装、更新插件。

8. **测试框架**：为插件系统编写 Mock `IPluginHost`，支持插件单元测试和集成测试。

9. **错误处理**：每个加载阶段失败都应有明确的错误码和人类可读的错误信息，方便插件开发者调试。

10. **线程安全**：PluginManager 的所有公共方法必须是线程安全的，内部使用读写锁保护注册表。