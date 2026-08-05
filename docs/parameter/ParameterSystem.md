# ParameterSystem — 参数系统设计

> **文档版本**: v1.0  
> **父文档**: [SystemArchitecture.md](../architecture/SystemArchitecture.md)  
> **关联模块**: ParamManagerService, DataBus, ConfigManager  

---

## 目标

设计一个工业级参数配置系统，为 Motor Studio 提供统一的参数读写、缓存、导入导出、版本管理和 UI 自动生成能力。参数系统需要屏蔽底层通信协议差异，提供类型安全的参数访问接口，并支持运行时不重启加载参数描述。

### 核心目标

1. **类型安全**：支持 8 种基础参数类型，编译期类型检查，运行时类型校验
2. **协议无关**：参数 ID 作为逻辑主键，物理地址仅作为传输映射
3. **高性能缓存**：两级缓存架构，减少 MCU 通信频次，支持批量操作
4. **可扩展**：WidgetFactory 插件化，新参数类型可扩展 UI 控件
5. **数据一致性**：双缓冲批量下载，失败自动回滚，参数版本校验
6. **热加载**：文件监控参数描述变更，不重启即可更新参数定义

---

## 设计原则

| 原则 | 说明 |
|------|------|
| **逻辑主键与物理地址分离** | 参数 ID 为系统内唯一标识，`address` 仅用于传输层寻址；修改通信协议不影响参数逻辑 |
| **核心层纯 C++，UI 层适配 Qt** | core 层使用 `std::variant`，仅在 UI/ViewModel 层转换为 `QVariant`；避免 Qt 类型污染核心 |
| **读写分离** | L1 读无锁（shared_lock），写独占（unique_lock）；L2 异步批量写入 |
| **单一数据源** | 参数描述文件（JSON）为唯一权威定义，代码不硬编码参数元数据 |
| **最小权限** | 参数支持读/写/管理员三级权限，UI 根据权限动态调整控件可用性 |
| **故障隔离** | 单个参数读写失败不影响其他参数；批量操作支持部分成功语义 |

---

## 类/模块关系

### 模块总览

```
┌──────────────────────────────────────────────────────────────────────────┐
│                           UI Layer (QML + C++)                            │
│  ┌──────────────────┐  ┌──────────────────┐  ┌──────────────────────────┐ │
│  │ ParameterPanel   │  │ ParamTreeView    │  │ ParamDiffView            │ │
│  │ (QML)            │  │ (QML)            │  │ (QML)                    │ │
│  └────────┬─────────┘  └────────┬─────────┘  └────────────┬─────────────┘ │
│           │                     │                          │               │
│  ┌────────▼─────────────────────▼──────────────────────────▼─────────────┐ │
│  │                    WidgetFactory (plugin-extensible)                    │ │
│  │  ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌─────────────┐ │ │
│  │  │BoolWidget│ │IntWidget │ │FloatWidget│ │EnumWidget│ │BitFieldWidget│ │ │
│  │  └──────────┘ └──────────┘ └──────────┘ └──────────┘ └─────────────┘ │ │
│  └───────────────────────────────────────────────────────────────────────┘ │
├───────────────────────────────────────────────────────────────────────────┤
│                        Service Layer                                       │
│  ┌───────────────────────────────────────────────────────────────────────┐ │
│  │                      ParamManagerService                               │ │
│  │  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐                 │ │
│  │  │ L1 ValueCache│  │ DiffEngine   │  │ ImportExport │                 │ │
│  │  │ (RWLock)     │  │              │  │ (JSON/CSV)   │                 │ │
│  │  └──────┬───────┘  └──────────────┘  └──────────────┘                 │ │
│  │         │                                                              │ │
│  │  ┌──────▼───────────────────────────────────────────────────────┐     │ │
│  │  │                  ParamDescriptorRegistry                       │     │ │
│  │  │  ┌──────────────┐  ┌──────────────┐  ┌──────────────────────┐ │     │ │
│  │  │  │ JSON Parser  │  │ FileWatcher  │  │ VersionChecker       │ │     │ │
│  │  │  └──────────────┘  └──────────────┘  └──────────────────────┘ │     │ │
│  │  └───────────────────────────────────────────────────────────────┘     │ │
│  └───────────────────────────────────────────────────────────────────────┘ │
├───────────────────────────────────────────────────────────────────────────┤
│                         Core Layer                                         │
│  ┌───────────────────────────────────────────────────────────────────────┐ │
│  │  ┌──────────────────┐  ┌──────────────────┐  ┌──────────────────────┐ │ │
│  │  │ IParameterProvider│  │ DoubleBuffer     │  │ ParamValue           │ │ │
│  │  │ (abstract)        │  │ (A/B partition)  │  │ (std::variant)       │ │ │
│  │  └──────────────────┘  └──────────────────┘  └──────────────────────┘ │ │
│  └───────────────────────────────────────────────────────────────────────┘ │
├───────────────────────────────────────────────────────────────────────────┤
│                      Protocol / Transport                                  │
│  ┌───────────────────────────────────────────────────────────────────────┐ │
│  │                      DataBus (CommandQueue)                            │ │
│  │  ┌──────────────┐  ┌──────────────┐  ┌──────────────────────────────┐ │ │
│  │  │ MCU Read/Write│  │ BatchCommand │  │ ResponseHandler              │ │ │
│  │  └──────────────┘  └──────────────┘  └──────────────────────────────┘ │ │
│  └───────────────────────────────────────────────────────────────────────┘ │
└───────────────────────────────────────────────────────────────────────────┘
```

### 核心类关系图

```
┌─────────────────────┐       owns        ┌─────────────────────────┐
│  ParamManagerService│ ─────────────────►│  ParamDescriptorRegistry │
│  (Singleton)        │                   │                          │
│  - registry_        │                   │  - descriptors_: map     │
│  - cache_           │                   │  - file_watcher_         │
│  - provider_        │                   │  - version_              │
└──────────┬──────────┘                   └──────────────────────────┘
           │                                        │
           │ owns                                   │ produces
           ▼                                        ▼
┌─────────────────────┐                   ┌─────────────────────────┐
│  L1ValueCache        │                   │  ParamDescriptor        │
│  - cache_: map       │                   │  - id: ParamId          │
│  - rwlock_: shared   │                   │  - name: string         │
│  - dirty_flags_      │                   │  - type: ParamType      │
└──────────┬──────────┘                   │  - address: uint32_t    │
           │                               │  - min/max/default      │
           │ delegates to                  │  - access: AccessLevel  │
           ▼                               │  - enum_values: map     │
┌─────────────────────┐                   └──────────────────────────┘
│  IParameterProvider  │ (abstract)
│  + readSingle()      │
│  + writeSingle()     │
│  + readBatch()       │
│  + writeBatch()      │
└─────────────────────┘
           △
           │ implements
           │
┌─────────────────────┐
│  McuParameterProvider│
│  - databus_          │
│  - protocol_adapter_ │
└─────────────────────┘
```

### 实例化策略

| 类 | 实例 | 线程 | 说明 |
|----|------|------|------|
| `ParamManagerService` | Singleton | WorkerPool | 参数操作入口，协调缓存与 Provider |
| `ParamDescriptorRegistry` | Singleton (owned) | Main Thread | 参数描述注册表，解析 JSON 描述文件 |
| `L1ValueCache` | Singleton (owned) | 多线程安全 (RWLock) | 一级缓存，所有参数当前值 |
| `IParameterProvider` | 可替换 | WorkerPool | L2 抽象接口，可 Mock 测试 |
| `McuParameterProvider` | 1 个/MCU 连接 | WorkerPool | L2 实现，通过 DataBus 与 MCU 通信 |
| `DoubleBuffer` | 临时对象 | WorkerPool | 批量下载时使用，操作完成后释放 |
| `WidgetFactory` | Singleton | Main Thread | UI 控件工厂，插件可注册新控件类型 |

---

## 数据流

### 上行：参数读取流程

```
UI 请求读取 ParamId
        │
        ▼
┌───────────────────┐
│  ParamManagerService│
│  ::readParameter() │
└────────┬──────────┘
         │
         ▼
┌───────────────────┐    cache hit?     ┌──────────────────┐
│  L1ValueCache      │─────────────────►│  return cached    │
│  shared_lock read  │                  │  value (O(1))     │
└────────┬──────────┘                   └──────────────────┘
         │ cache miss / force refresh
         ▼
┌───────────────────┐
│  IParameterProvider│
│  ::readSingle()    │
└────────┬──────────┘
         │
         ▼
┌───────────────────┐
│  McuParameterProvider                 │
│  1. lookup address by ParamId        │
│  2. enqueue Command (Priority 2)     │
│  3. wait for response (timeout 500ms)│
└────────┬──────────┘
         │
         ▼
┌───────────────────┐
│  DataBus           │
│  → Protocol → MCU  │
└────────┬──────────┘
         │ response
         ▼
┌───────────────────┐
│  L1ValueCache      │
│  unique_lock write │  ← 更新缓存
└────────┬──────────┘
         │
         ▼
   return ParamValue
```

### 下行：参数写入流程

```
UI 请求写入 ParamId = newValue
        │
        ▼
┌──────────────────────────┐
│  ParamManagerService      │
│  ::writeParameter()       │
│  1. 权限检查 (AccessLevel)│
│  2. 范围校验 (min/max)    │
│  3. 类型校验              │
└────────┬─────────────────┘
         │
         ▼
┌──────────────────────────┐
│  L1ValueCache             │
│  unique_lock write        │  ← 乐观更新 L1（先写缓存）
│  mark dirty               │
└────────┬─────────────────┘
         │
         ▼
┌──────────────────────────┐
│  IParameterProvider        │
│  ::writeSingle()           │
└────────┬─────────────────┘
         │
         ▼
┌──────────────────────────┐
│  McuParameterProvider      │
│  1. 编码参数值 → 协议帧   │
│  2. enqueue (Priority 1)  │
│  3. 等待 MCU 确认         │
└────────┬─────────────────┘
         │
    ┌────┴────┐
    │         │
    ▼         ▼
  成功      失败
    │         │
    │         ▼
    │    ┌──────────────┐
    │    │ 回滚 L1 缓存  │  ← 恢复旧值
    │    │ clear dirty   │
    │    │ emit error    │
    │    └──────────────┘
    ▼
┌──────────────┐
│ clear dirty   │
│ emit changed  │
└──────────────┘
```

### 批量下载流程（双缓冲）

```
┌──────────────────────────────────────────────────────────────────┐
│                    Batch Download (Double Buffer)                  │
│                                                                    │
│  请求批量下载 [ParamId_1 ... ParamId_N]                            │
│       │                                                            │
│       ▼                                                            │
│  ┌──────────────────────────────────────────────────────────┐     │
│  │              DoubleBuffer<ParamValue>                      │     │
│  │                                                            │     │
│  │  ┌──────────────────┐     ┌──────────────────┐            │     │
│  │  │  Partition A     │     │  Partition B     │            │     │
│  │  │  (active, read)  │     │  (staging, write)│            │     │
│  │  │  current values  │     │  new values      │            │     │
│  │  └──────────────────┘     └────────┬─────────┘            │     │
│  │                                    │                       │     │
│  │  1. 写入 Partition B (staging)     │                       │     │
│  │     ├─ 逐个从 MCU 读取             │                       │     │
│  │     ├─ 校验每个参数 (范围/类型)    │                       │     │
│  │     └─ 全部成功后:                 │                       │     │
│  │         ├─ swap A ↔ B             │                       │     │
│  │         └─ 更新 L1 缓存            │                       │     │
│  │  2. 任何失败:                      │                       │     │
│  │     ├─ 丢弃 Partition B            │                       │     │
│  │     ├─ A 保持不变                  │                       │     │
│  │     └─ 返回错误 + 失败参数列表     │                       │     │
│  └──────────────────────────────────────────────────────────┘     │
└──────────────────────────────────────────────────────────────────┘
```

### 参数描述热加载流程

```
┌──────────────────┐
│  FileWatcher      │  (QFileSystemWatcher / inotify)
│  监控 params/*.json│
└────────┬─────────┘
         │ 文件变更事件
         ▼
┌──────────────────┐
│  debounce 500ms  │  (合并连续写入)
└────────┬─────────┘
         │
         ▼
┌──────────────────────────────────────┐
│  ParamDescriptorRegistry              │
│  ::reload(file_path)                  │
│  1. 解析 JSON → 临时 Descriptor Map  │
│  2. 校验新描述 (完整性/无冲突)       │
│  3. 参数版本比对 (param_version)     │
│     ├─ 版本相同 → 跳过               │
│     └─ 版本不同 → 继续               │
│  4. atomic swap descriptor map       │
│  5. 通知 WidgetFactory 重建 UI       │
│  6. L1 缓存标记对应参数为 stale      │
└──────────────────────────────────────┘
```

---

## 参数类型定义

### ParamType 枚举

```cpp
// 核心层类型定义 (无 Qt 依赖)
enum class ParamType : uint8_t {
    Bool       = 0x00,
    Int8       = 0x01,
    Int16      = 0x02,
    Int32      = 0x03,
    UInt8      = 0x04,
    UInt16     = 0x05,
    UInt32     = 0x06,
    Float32    = 0x07,
    Enum       = 0x08,
    BitField   = 0x09,
    String     = 0x0A,
    // 预留扩展
    Float64    = 0x0B,  // 未来
    Blob       = 0x0C,  // 未来
};
```

### ParamValue (std::variant)

```cpp
// 核心层: 使用 std::variant，无 Qt 依赖
using ParamValue = std::variant<
    bool,
    int8_t, int16_t, int32_t,
    uint8_t, uint16_t, uint32_t,
    float,
    std::string,
    EnumValue,       // 枚举值: {uint32_t raw; std::string label;}
    BitFieldValue    // 位域值: {uint32_t raw; std::bitset<32> bits;}
>;

// UI 层: QVariant 转换桥
class ParamValueBridge {
public:
    static QVariant toQVariant(const ParamValue& v);
    static ParamValue fromQVariant(const QVariant& v, ParamType expected_type);
};
```

### ParamId 定义

```cpp
// 参数逻辑主键，与物理地址解耦
using ParamId = uint32_t;

// 参数 ID 命名空间分段
namespace ParamIds {
    constexpr ParamId SYSTEM_BASE    = 0x0000'0000;
    constexpr ParamId MOTOR_BASE     = 0x0001'0000;
    constexpr ParamId DRIVER_BASE    = 0x0002'0000;
    constexpr ParamId PID_BASE       = 0x0003'0000;
    constexpr ParamId PROTECTION_BASE= 0x0004'0000;
    constexpr ParamId COMMUNICATION  = 0x0005'0000;
    constexpr ParamId USER_BASE      = 0x8000'0000;  // 用户自定义
}
```

---

## JSON 参数描述文件格式

### 完整 Schema

```json
{
  "$schema": "https://motor-studio/schemas/parameter-description/v1",
  "meta": {
    "device_model": "Motor-Drive-X1",
    "firmware_version": "2.3.1",
    "param_version": 15,
    "description": "X1 系列电机驱动参数定义",
    "last_modified": "2026-08-01T10:00:00Z",
    "author": "Firmware Team"
  },
  "parameters": [
    {
      "id": 65536,
      "name": "MotorMaxSpeed",
      "display_name": "电机最高转速",
      "display_name_en": "Motor Max Speed",
      "type": "UInt16",
      "address": {
        "protocol": "modbus",
        "register": 256,
        "bit_offset": 0,
        "bit_width": 16,
        "endian": "big"
      },
      "default_value": 3000,
      "min_value": 0,
      "max_value": 6000,
      "unit": "RPM",
      "scale": 1.0,
      "offset": 0.0,
      "access": "rw",
      "persistence": "flash",
      "category": "motor",
      "group": "speed_control",
      "description": "设置电机允许的最高转速。超过此值触发过速保护。",
      "description_en": "Maximum allowed motor speed. Overspeed protection triggers above this value.",
      "tags": ["critical", "startup_config"],
      "depends_on": [],
      "affects": ["MotorOverSpeedAlarm"],
      "update_rate_hz": 0,
      "validation": {
        "custom_rule": "value % 10 == 0",
        "custom_message": "转速必须为10的整数倍"
      }
    },
    {
      "id": 65537,
      "name": "ControlMode",
      "display_name": "控制模式",
      "type": "Enum",
      "address": {
        "protocol": "modbus",
        "register": 257
      },
      "default_value": 0,
      "access": "rw",
      "persistence": "flash",
      "category": "motor",
      "group": "control",
      "enum_values": [
        {"value": 0, "label": "速度控制", "label_en": "Speed Control"},
        {"value": 1, "label": "转矩控制", "label_en": "Torque Control"},
        {"value": 2, "label": "位置控制", "label_en": "Position Control"},
        {"value": 3, "label": "混合控制", "label_en": "Hybrid Control"}
      ],
      "description": "电机控制模式选择。切换模式时电机会先停止。"
    },
    {
      "id": 65538,
      "name": "FaultStatus",
      "display_name": "故障状态字",
      "type": "BitField",
      "address": {
        "protocol": "modbus",
        "register": 258
      },
      "default_value": 0,
      "access": "ro",
      "category": "diagnostic",
      "group": "faults",
      "bit_fields": [
        {"bit": 0, "name": "OverCurrent", "label": "过流", "severity": "critical"},
        {"bit": 1, "name": "OverVoltage", "label": "过压", "severity": "critical"},
        {"bit": 2, "name": "OverTemperature", "label": "过温", "severity": "warning"},
        {"bit": 3, "name": "EncoderFault", "label": "编码器故障", "severity": "critical"},
        {"bit": 4, "name": "CommunicationLoss", "label": "通信丢失", "severity": "warning"},
        {"bit": 5, "name": "UnderVoltage", "label": "欠压", "severity": "warning"},
        {"bits": [6, 7], "name": "Reserved", "label": "保留"}
      ],
      "description": "电机故障状态位。系统根据严重程度自动执行保护动作。"
    },
    {
      "id": 65539,
      "name": "DeviceSerialNumber",
      "display_name": "设备序列号",
      "type": "String",
      "address": {
        "protocol": "modbus",
        "register": 260,
        "length_registers": 8
      },
      "max_length": 16,
      "default_value": "",
      "access": "ro",
      "persistence": "factory",
      "category": "system",
      "group": "identity",
      "description": "设备出厂序列号，不可修改。"
    }
  ],
  "groups": [
    {
      "id": "speed_control",
      "display_name": "速度控制",
      "display_order": 1
    },
    {
      "id": "control",
      "display_name": "控制模式",
      "display_order": 2
    },
    {
      "id": "faults",
      "display_name": "故障诊断",
      "display_order": 10
    }
  ],
  "categories": [
    {
      "id": "motor",
      "display_name": "电机参数",
      "display_order": 1
    },
    {
      "id": "diagnostic",
      "display_name": "诊断参数",
      "display_order": 5
    },
    {
      "id": "system",
      "display_name": "系统参数",
      "display_order": 10
    }
  ]
}
```

### Schema 字段说明

| 字段 | 类型 | 必需 | 说明 |
|------|------|------|------|
| `meta.param_version` | uint32 | 是 | 参数集版本号，MCU 与上位机不一致时提示升级 |
| `meta.device_model` | string | 是 | 设备型号，用于匹配参数文件 |
| `meta.firmware_version` | string | 是 | 固件版本，用于兼容性检查 |
| `parameters[].id` | uint32 | 是 | 参数逻辑主键，全局唯一 |
| `parameters[].type` | enum | 是 | 参数类型：Bool, Int8, Int16, Int32, UInt8, UInt16, UInt32, Float32, Enum, BitField, String |
| `parameters[].address.protocol` | string | 是 | 协议类型：modbus, canopen, ethercat, custom |
| `parameters[].address.register` | uint32 | 是 | 寄存器/对象地址 |
| `parameters[].access` | enum | 是 | 访问权限：ro (只读), rw (读写), wo (只写), admin (管理员) |
| `parameters[].persistence` | enum | 否 | 持久化类型：ram (掉电丢失), flash (掉电保存), factory (出厂固化) |
| `parameters[].scale` | float | 否 | 缩放因子，实际值 = raw_value × scale + offset |
| `parameters[].default_value` | variant | 否 | 出厂默认值 |
| `parameters[].min_value` | number | 否 | 允许最小值 |
| `parameters[].max_value` | number | 否 | 允许最大值 |
| `parameters[].enum_values` | array | Enum 必需 | 枚举值定义列表 |
| `parameters[].bit_fields` | array | BitField 必需 | 位域定义列表 |
| `parameters[].category` | string | 是 | 参数分类，对应 categories |
| `parameters[].group` | string | 是 | 参数分组，对应 groups |
| `parameters[].depends_on` | array | 否 | 依赖的其他参数 ID 列表 |
| `parameters[].validation.custom_rule` | string | 否 | 自定义校验表达式 |
| `parameters[].tags` | array | 否 | 标签：critical, startup_config, advanced, hidden |

---

## 两级缓存架构

### L1: ValueCache（值缓存）

```cpp
class L1ValueCache {
public:
    // 单参数读取 (O(1), shared_lock)
    std::optional<ParamValue> get(ParamId id) const;
    
    // 单参数写入 (unique_lock)
    void put(ParamId id, ParamValue value);
    
    // 批量读取 (shared_lock, 一次加锁)
    std::vector<std::optional<ParamValue>> getBatch(const std::vector<ParamId>& ids) const;
    
    // 批量写入 (unique_lock, 原子操作)
    void putBatch(const std::vector<std::pair<ParamId, ParamValue>>& updates);
    
    // 标记脏数据 (需刷新到 MCU)
    bool isDirty(ParamId id) const;
    void markDirty(ParamId id);
    void clearDirty(ParamId id);
    
    // 标记过期 (参数描述变更后)
    void markStale(ParamId id);
    bool isStale(ParamId id) const;
    
    // 全量操作
    void clear();
    size_t size() const;
    std::vector<ParamId> getDirtyParams() const;

private:
    struct CacheEntry {
        ParamValue value;
        uint64_t timestamp;        // 最后更新时间
        uint32_t source_version;   // 参数描述版本（用于 stale 检测）
        bool dirty : 1;
        bool stale : 1;
    };
    
    mutable std::shared_mutex rwlock_;
    std::unordered_map<ParamId, CacheEntry> cache_;
};
```

### L2: IParameterProvider（MCU 通信抽象）

```cpp
class IParameterProvider {
public:
    virtual ~IParameterProvider() = default;
    
    // 单参数读写
    virtual Result<ParamValue> readSingle(ParamId id, 
                                           std::chrono::milliseconds timeout = 500ms) = 0;
    virtual Result<void> writeSingle(ParamId id, 
                                      const ParamValue& value,
                                      std::chrono::milliseconds timeout = 500ms) = 0;
    
    // 批量读取 (优化: 一次通信读取多个寄存器)
    virtual Result<std::vector<ParamValue>> readBatch(
        const std::vector<ParamId>& ids,
        std::chrono::milliseconds timeout = 2000ms) = 0;
    
    // 批量写入
    virtual Result<void> writeBatch(
        const std::vector<std::pair<ParamId, ParamValue>>& updates,
        std::chrono::milliseconds timeout = 2000ms) = 0;
    
    // 批量下载 (使用双缓冲)
    virtual Result<BatchDownloadResult> downloadBatch(
        const std::vector<ParamId>& ids,
        DownloadCallback progress_callback = {}) = 0;
    
    // 健康检查
    virtual bool isConnected() const = 0;
    virtual uint32_t getLatencyMs() const = 0;
    
    // 参数版本
    virtual uint32_t getMcUParamVersion() const = 0;
};
```

---

## API 接口规划

### ParamManagerService 公共接口

```cpp
class ParamManagerService {
public:
    // ====== 生命周期 ======
    void init(const std::filesystem::path& param_desc_dir,
              std::shared_ptr<IParameterProvider> provider);
    void shutdown();
    
    // ====== 参数描述管理 ======
    Result<void> loadParameterDescriptions(const std::filesystem::path& json_path);
    Result<void> reloadParameterDescriptions();  // 热加载
    const ParamDescriptor* getDescriptor(ParamId id) const;
    std::vector<const ParamDescriptor*> getDescriptorsByCategory(std::string_view category) const;
    std::vector<const ParamDescriptor*> getDescriptorsByGroup(std::string_view group) const;
    std::vector<const ParamDescriptor*> getAllDescriptors() const;
    uint32_t getParameterVersion() const;
    
    // ====== 参数读写 ======
    // 单参数读取 (L1 缓存优先)
    Result<ParamValue> readParameter(ParamId id, bool force_refresh = false);
    // 单参数写入
    Result<void> writeParameter(ParamId id, const ParamValue& value);
    // 批量读取
    Result<std::vector<ParamValue>> readParameters(const std::vector<ParamId>& ids);
    // 批量写入
    Result<void> writeParameters(const std::vector<std::pair<ParamId, ParamValue>>& updates);
    // 批量下载 (从 MCU 全量同步)
    Result<BatchDownloadResult> downloadAllParameters(DownloadProgressCallback cb = {});
    Result<BatchDownloadResult> downloadParameters(const std::vector<ParamId>& ids, 
                                                     DownloadProgressCallback cb = {});
    
    // ====== 导入导出 ======
    Result<std::string> exportToJson(const std::vector<ParamId>& ids = {}) const;
    Result<void> importFromJson(std::string_view json_content, bool simulate = false);
    Result<std::string> exportToCsv(const std::vector<ParamId>& ids = {}) const;
    Result<void> importFromCsv(std::string_view csv_content, bool simulate = false);
    
    // ====== 参数比较 ======
    ParamDiffResult diff(const std::vector<ParamId>& ids,
                         const std::unordered_map<ParamId, ParamValue>& other) const;
    ParamDiffResult diffWithFile(const std::filesystem::path& json_path) const;
    ParamDiffResult diffWithMcU() const;
    
    // ====== 批量操作 ======
    // 将 L1 脏数据刷写到 MCU
    Result<void> flushDirtyParameters();
    // 将所有参数恢复为默认值
    Result<void> restoreDefaults(const std::vector<ParamId>& ids = {});
    
    // ====== 信号 ======
    Signal<ParamId, ParamValue> onParameterChanged;       // 参数值变更
    Signal<> onParametersReloaded;                         // 参数描述热加载完成
    Signal<ParamId, std::string> onParameterError;         // 参数操作错误
    Signal<double> onDownloadProgress;                     // 批量下载进度 0.0-1.0
    Signal<ParamDiffResult> onVersionMismatch;             // 参数版本不匹配
};

// 结果类型
template<typename T>
class Result {
public:
    bool isOk() const;
    const T& value() const;
    const std::string& error() const;
    // 部分成功: 批量操作返回
    const std::vector<ParamId>& failedParams() const;
};

// 参数比较结果
struct ParamDiffResult {
    std::vector<ParamId> added;         // 新增参数
    std::vector<ParamId> removed;       // 移除参数
    std::vector<ParamDiffEntry> changed; // 变更参数
    bool hasDifference() const;
};

struct ParamDiffEntry {
    ParamId id;
    ParamValue old_value;
    ParamValue new_value;
    std::string display_name;
};
```

### WidgetFactory 接口

```cpp
// UI 控件工厂 — 插件化扩展
class IParamWidget {
public:
    virtual ~IParamWidget() = default;
    
    virtual QWidget* createWidget(QWidget* parent) = 0;
    virtual void setValue(const ParamValue& value) = 0;
    virtual ParamValue getValue() const = 0;
    virtual void setReadOnly(bool read_only) = 0;
    virtual void setDescriptor(const ParamDescriptor& desc) = 0;
    
    virtual bool validate() const;  // UI 层校验
    virtual QString validationError() const;
    
    Signal<ParamValue> valueChanged;  // 用户交互变更
};

class WidgetFactory {
public:
    static WidgetFactory& instance();
    
    // 注册控件类型
    using WidgetCreator = std::function<std::unique_ptr<IParamWidget>()>;
    void registerWidget(ParamType type, WidgetCreator creator);
    void registerWidget(const std::string& custom_type, WidgetCreator creator);
    
    // 创建控件
    std::unique_ptr<IParamWidget> createWidget(const ParamDescriptor& desc, QWidget* parent);
    
    // 查询
    bool hasWidget(ParamType type) const;
    std::vector<ParamType> supportedTypes() const;
};
```

---

## 参数版本检查

```cpp
class ParamVersionChecker {
public:
    enum class VersionAction {
        Compatible,          // 版本一致，正常使用
        UpgradeAvailable,    // 有新版参数描述，提示用户升级
        DowngradeDetected,   // 参数描述版本低于 MCU，警告
        Incompatible,        // 主版本不兼容，拒绝加载
    };
    
    struct VersionInfo {
        uint32_t file_version;     // 参数描述文件版本
        uint32_t mcu_version;      // MCU 固件参数版本
        uint32_t last_synced;      // 上次同步的版本
        std::string device_model;
        std::string firmware_version;
    };
    
    VersionAction check(const VersionInfo& info);
    bool needsResync(const VersionInfo& info);
};
```

---

## 双缓冲批量下载

```cpp
template<typename T>
class DoubleBuffer {
public:
    explicit DoubleBuffer(size_t capacity);
    
    // 写入 staging buffer
    void stageWrite(size_t index, T value);
    T& stageAt(size_t index);
    
    // 提交: 原子交换 active ↔ staging
    void commit();
    
    // 回滚: 丢弃 staging
    void rollback();
    
    // 从 active buffer 读取
    const T& read(size_t index) const;
    const T* activeData() const;
    size_t activeSize() const;
    
    // 状态
    bool isCommitted() const;
    size_t capacity() const;

private:
    std::vector<T> buffer_a_;  // active
    std::vector<T> buffer_b_;  // staging
    std::vector<T>* active_;
    std::vector<T>* staging_;
    bool committed_ = false;
};
```

---

## 文件监控（热加载）

```cpp
class ParamFileWatcher {
public:
    explicit ParamFileWatcher(const std::filesystem::path& directory);
    ~ParamFileWatcher();
    
    void start();
    void stop();
    
    using ReloadCallback = std::function<void(const std::filesystem::path& changed_file)>;
    void setReloadCallback(ReloadCallback cb);
    
    // 配置
    void setDebounceMs(std::chrono::milliseconds ms);
    void addWatchPattern(const std::string& glob_pattern);  // 默认 "*.json"

private:
    std::filesystem::path watch_dir_;
    // 实现: Windows → ReadDirectoryChangesW, Linux → inotify
    // 或使用 QFileSystemWatcher (Qt 封装)
    std::unique_ptr<FileWatcherImpl> impl_;
    std::chrono::milliseconds debounce_ms_{500};
};
```

---

## 后续实现注意事项

1. **std::variant vs QVariant 边界**：核心层（`ParamValue`、`L1ValueCache`、`IParameterProvider`）严格使用 `std::variant`，不依赖 Qt。`ParamValueBridge` 在 Service/UI 边界完成转换。这确保核心层可独立编译、测试，甚至移植到非 Qt 平台。

2. **参数描述 JSON 解析性能**：1000+ 参数时，JSON 解析可能耗时 100ms+。使用 `simdjson` 或 `rapidjson` 替代 `nlohmann/json` 加速解析；解析结果缓存二进制格式（`ParamDescriptorCache.bin`），热加载时先读缓存。

3. **L1 缓存内存占用**：假设 2000 个参数，每个 `CacheEntry` 约 64 字节，总内存 ~128KB。`std::variant` 中 `std::string` 类型可能导致堆分配，建议对常用 String 参数使用 `SmallString<32>` 优化。

4. **批量读取的寄存器合并优化**：`McuParameterProvider` 应分析请求的参数地址列表，合并相邻寄存器为一次多寄存器读取（如 Modbus 功能码 03 批量读取），减少通信次数。

5. **双缓冲的线程安全**：`DoubleBuffer::commit()` 中的 swap 必须是原子的。使用 `std::atomic<std::vector<T>*>` 或 `std::atomic<size_t>` 分区索引实现无锁切换。

6. **参数依赖处理**：`depends_on` 字段表示当前参数依赖其他参数。当依赖参数变更时，应自动刷新当前参数值。在 UI 中，依赖参数变更后应 disable 被依赖参数的控件，防止配置不一致。

7. **自定义校验规则**：`validation.custom_rule` 使用表达式引擎（如 `exprtk`）。校验表达式需在独立沙箱中执行，防止恶意表达式导致崩溃。支持的运算符：`+`, `-`, `*`, `/`, `%`, `==`, `!=`, `<`, `>`, `<=`, `>=`, `&&`, `||`, `!`。

8. **导入模拟 (dry-run)**：`importFromJson(json, simulate=true)` 在正式导入前执行全量校验，返回所有错误但不实际写入。用于 UI 预览导入结果。

9. **参数变更审计日志**：所有参数写入操作记录带时间戳的审计日志：`[timestamp] [user] write ParamId=XXX, old=YYY, new=ZZZ, result=success/failure`。敏感参数（标记 `sensitive` tag）值脱敏为 `[REDACTED]`。

10. **WidgetFactory 插件注册**：内置控件（Bool/Int/Float/Enum/BitField/String）在 `WidgetFactory` 构造时注册。外部插件通过 `PluginLoader` 加载后调用 `WidgetFactory::registerWidget()` 注册。使用 `std::unordered_map<ParamType, WidgetCreator>` 存储，支持自定义类型字符串。

11. **参数 ID 冲突检测**：CMake 构建脚本扫描所有 JSON 参数描述文件，检测 `id` 重复。运行时 `ParamDescriptorRegistry::load()` 也执行检测，重复 ID 拒绝加载并报错。

12. **大规模参数列表的 UI 虚拟化**：2000+ 参数时，`ParamTreeView` 使用 Qt 的 Model/View 框架 + `QAbstractItemModel::canFetchMore()` 实现虚拟滚动，只渲染可见节点。

13. **scale/offset 精度**：`scale` 和 `offset` 使用 `double` 类型，避免浮点累积误差。转换公式：`physical_value = raw_value * scale + offset`。反向转换：`raw_value = round((physical_value - offset) / scale)`。

14. **参数持久化类型语义**：`ram` — 仅在上位机缓存中，掉电丢失；`flash` — MCU 侧持久化，上位机可读写；`factory` — MCU 出厂固化，上位机只读，不可修改。