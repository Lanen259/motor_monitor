# 消息格式规范 (Message Format Specification)

> 版本: 1.0 | 状态: 设计阶段 | 作者: 系统架构组

---

## 1. 目标 (Goal)

定义电机监控系统中上位机与下位机之间所有消息的**精确二进制格式**，确保：

- **命令码统一**：所有命令码在 `CommandCode` 枚举中集中定义，无歧义。
- **数据类型明确**：每种数据类型有固定的编码规则，无隐式转换。
- **流数据高效**：多变量流数据帧紧凑编码，支持高频传输。
- **版本兼容**：消息格式变更时有明确的兼容性规则，旧版本固件可优雅降级。
- **可扩展**：预留扩展空间，支持未来新增命令和变量。

---

## 2. 设计原则 (Design Principles)

| 原则 | 说明 |
|------|------|
| **小端序** | 所有多字节数值使用小端序（与 ARM Cortex-M 一致），避免字节序转换 |
| **显式类型** | 每个变量在注册表中定义其 `DataType`，解析时不做隐式推断 |
| **紧凑优先** | 流数据使用 `variable_id` 而非名称，减少传输开销 |
| **固定布局** | 每种消息类型的载荷格式固定，不依赖自描述标签（如 JSON） |
| **向前兼容** | 新增字段追加到载荷末尾，旧版本忽略未知尾部字段 |
| **对齐** | 多字节字段自然对齐，避免非对齐访问（ARM Cortex-M 允许非对齐但性能下降） |

---

## 3. 类/模块关系 (Class/Module Relationships)

### 3.1 模块关系图

```
┌──────────────────────────────────────────────────────────┐
│                    Message Layer                          │
│                                                          │
│  ┌──────────────────┐    ┌────────────────────────────┐  │
│  │  MessageSerializer│    │  VariableRegistry          │  │
│  │  (序列化/反序列化) │    │  (变量定义注册表)           │  │
│  │                   │    │                            │  │
│  │  + serialize()    │───▶│  + registerVariable()      │  │
│  │  + deserialize()  │    │  + lookup(id)              │  │
│  │  + validateSize() │    │  + lookup(name)            │  │
│  └──────────────────┘    │  + variablesByGroup()       │  │
│                          └────────────────────────────┘  │
│  ┌──────────────────┐                                   │
│  │  CommandCode      │    ┌────────────────────────────┐  │
│  │  (命令码枚举)      │    │  ErrorCode                 │  │
│  │                   │    │  (错误码枚举)               │  │
│  │  enum class:      │    │                            │  │
│  │   uint16_t        │    │  enum class: uint8_t       │  │
│  └──────────────────┘    └────────────────────────────┘  │
│                                                          │
│  ┌──────────────────┐    ┌────────────────────────────┐  │
│  │  DataType         │    │  VariableValue             │  │
│  │  (数据类型枚举)    │    │  (变量值结构体)             │  │
│  │                   │    │                            │  │
│  │  enum class:      │    │  + variableId: uint16_t    │  │
│  │   uint8_t         │    │  + dataType: DataType      │  │
│  └──────────────────┘    │  + value: QVariant          │  │
│                          │  + timestamp: uint64_t      │  │
│                          └────────────────────────────┘  │
└──────────────────────────────────────────────────────────┘
```

### 3.2 核心类说明

| 类 | 职责 | 依赖 |
|----|------|------|
| `MessageSerializer` | 将高层消息对象序列化为字节流，或从字节流反序列化 | `VariableRegistry`, `DataType` |
| `VariableRegistry` | 变量 ID ↔ 名称 ↔ 类型定义 的映射表 | `VariableDefinition` |
| `CommandCode` | 枚举所有命令码，集中管理，避免魔法数字 | 无 |
| `DataType` | 枚举所有数据类型，定义每种类型的字节大小 | 无 |
| `ErrorCode` | 枚举所有错误码，定义下位机响应状态 | 无 |
| `VariableDefinition` | 单个变量的完整定义（ID、名称、类型、单位、缩放等） | 无 |
| `VariableValue` | 运行时变量值，携带时间戳和元数据 | 无 |

### 3.3 VariableValue 结构体

```cpp
struct VariableValue {
    uint16_t   variableId;   // 变量 ID
    DataType   dataType;     // 数据类型
    QVariant   value;        // 实际值 (使用 QVariant 统一承载)
    uint64_t   timestamp;    // 微秒时间戳
    DataQuality quality;      // 数据质量标记

    // 便捷方法
    float   toFloat()   const;
    int32_t toInt32()   const;
    bool    toBool()    const;
    QString toString()  const;
};

enum class DataQuality : uint8_t {
    Good        = 0x00,
    Stale       = 0x01,   // 数据陈旧
    Interpolated = 0x02,  // 插值填充
    Invalid     = 0xFF,
};
```

---

## 4. 命令码表 (Command Code Table)

### 3.1 地址空间划分

```
16-bit 命令码: 0x0000 - 0xFFFF
├── 0x0000 - 0x0FFF: 系统命令 (System Commands)
├── 0x1000 - 0x1FFF: 参数读写 (Parameter Read/Write)
├── 0x2000 - 0x2FFF: 电机控制 (Motor Control)
├── 0x3000 - 0x3FFF: 诊断与状态 (Diagnostics & Status)
├── 0x4000 - 0x4FFF: 固件升级 (Firmware Update)
├── 0x5000 - 0x5FFF: 数据记录 (Data Logging)
├── 0x6000 - 0x7FFF: 保留 (Reserved)
├── 0x8000 - 0x8FFF: 实时变量流 (Real-time Variable Stream)
├── 0x9000 - 0x9FFF: 波形数据流 (Waveform Data Stream)
└── 0xA000 - 0xFFFF: 保留 (Reserved)
```

### 3.2 系统命令 (0x0000 - 0x0FFF)

| 命令码 | 名称 | 方向 | 描述 |
|--------|------|------|------|
| 0x0000 | `CMD_NOP` | 双向 | 空操作 / 心跳 |
| 0x0001 | `CMD_VERSION_QUERY` | 上行 | 查询协议版本 |
| 0x0002 | `CMD_VERSION_CONFIRM` | 上行 | 确认协议版本 |
| 0x0003 | `CMD_RESET` | 上行 | 软复位下位机 |
| 0x0004 | `CMD_DEVICE_INFO` | 上行 | 查询设备信息 |
| 0x0005 | `CMD_SELF_TEST` | 上行 | 触发自检 |
| 0x0006 | `CMD_ENTER_BOOTLOADER` | 上行 | 进入 Bootloader |
| 0x0007 | `CMD_SYNC_TIME` | 上行 | 同步时间戳 |
| 0x00FF | `CMD_FRAGMENT` | 双向 | 大包分片传输 |

### 3.3 参数读写 (0x1000 - 0x1FFF)

| 命令码 | 名称 | 方向 | 描述 |
|--------|------|------|------|
| 0x1001 | `CMD_READ_PARAM` | 上行 | 读取单个参数 |
| 0x1002 | `CMD_WRITE_PARAM` | 上行 | 写入单个参数 |
| 0x1003 | `CMD_READ_PARAMS` | 上行 | 批量读取参数 |
| 0x1004 | `CMD_WRITE_PARAMS` | 上行 | 批量写入参数 |
| 0x1005 | `CMD_SAVE_PARAMS` | 上行 | 保存参数到 Flash |
| 0x1006 | `CMD_LOAD_DEFAULTS` | 上行 | 恢复出厂设置 |

### 3.4 电机控制 (0x2000 - 0x2FFF)

| 命令码 | 名称 | 方向 | 描述 |
|--------|------|------|------|
| 0x2001 | `CMD_MOTOR_ENABLE` | 上行 | 使能电机 |
| 0x2002 | `CMD_MOTOR_DISABLE` | 上行 | 禁用电机 |
| 0x2003 | `CMD_MOTOR_EMERGENCY_STOP` | 上行 | 紧急停止 |
| 0x2004 | `CMD_SET_TARGET_SPEED` | 上行 | 设置目标转速 |
| 0x2005 | `CMD_SET_TARGET_POSITION` | 上行 | 设置目标位置 |
| 0x2006 | `CMD_SET_TARGET_TORQUE` | 上行 | 设置目标转矩 |
| 0x2007 | `CMD_SET_CONTROL_MODE` | 上行 | 切换控制模式 |
| 0x2008 | `CMD_SET_PID_PARAMS` | 上行 | 设置 PID 参数 |
| 0x2009 | `CMD_START_PROFILE` | 上行 | 启动运动曲线 |
| 0x200A | `CMD_STOP_PROFILE` | 上行 | 停止运动曲线 |
| 0x200B | `CMD_CLEAR_FAULT` | 上行 | 清除故障 |

### 3.5 诊断与状态 (0x3000 - 0x3FFF)

| 命令码 | 名称 | 方向 | 描述 |
|--------|------|------|------|
| 0x3001 | `CMD_READ_STATUS` | 上行 | 读取运行状态 |
| 0x3002 | `CMD_READ_FAULT_LOG` | 上行 | 读取故障日志 |
| 0x3003 | `CMD_READ_TEMPERATURE` | 上行 | 读取温度 |
| 0x3004 | `CMD_READ_CURRENT` | 上行 | 读取电流 |
| 0x3005 | `CMD_READ_VOLTAGE` | 上行 | 读取电压 |
| 0x3006 | `CMD_READ_ENCODER` | 上行 | 读取编码器值 |
| 0x3007 | `CMD_START_SCOPE` | 上行 | 启动示波器模式 |
| 0x3008 | `CMD_STOP_SCOPE` | 上行 | 停止示波器模式 |

### 3.6 固件升级 (0x4000 - 0x4FFF)

| 命令码 | 名称 | 方向 | 描述 |
|--------|------|------|------|
| 0x4001 | `CMD_FW_ERASE` | 上行 | 擦除 Flash 区域 |
| 0x4002 | `CMD_FW_WRITE_CHUNK` | 上行 | 写入升级数据块 |
| 0x4003 | `CMD_FW_VERIFY` | 上行 | 校验升级数据 |
| 0x4004 | `CMD_FW_COMMIT` | 上行 | 提交升级 |
| 0x4005 | `CMD_FW_GET_STATUS` | 上行 | 查询升级状态 |

### 3.7 流数据标识 (0x8000 - 0xFFFF)

流数据帧不使用独立的命令码，而是通过 `CMD` 字段范围区分：

| 范围 | 用途 |
|------|------|
| 0x8001 | 实时变量流（多变量采样数据） |
| 0x9001 | 电流波形流（ADC 原始波形） |
| 0x9002 | 速度波形流 |
| 0x9003 | 位置波形流 |

---

## 5. 数据类型编码 (Data Type Encoding)

### 4.1 数据类型枚举

```cpp
enum class DataType : uint8_t {
    // 整数类型
    Int8    = 0x01,   // 有符号 8 位
    Int16   = 0x02,   // 有符号 16 位
    Int32   = 0x03,   // 有符号 32 位
    UInt8   = 0x11,   // 无符号 8 位
    UInt16  = 0x12,   // 无符号 16 位
    UInt32  = 0x13,   // 无符号 32 位

    // 浮点类型
    Float32 = 0x21,   // IEEE 754 单精度 (4 字节)
    
    // 特殊类型
    Bool    = 0x31,   // 布尔 (1 字节, 0x00=false, 0x01=true)
    Enum8   = 0x41,   // 枚举 (1 字节)
    Enum16  = 0x42,   // 枚举 (2 字节)
    Bitfield8  = 0x51,  // 位域 (1 字节)
    Bitfield16 = 0x52,  // 位域 (2 字节)
    Bitfield32 = 0x53,  // 位域 (4 字节)

    // 复合类型
    String  = 0x61,   // UTF-8 字符串 (1B 长度前缀 + 数据)
    Bytes   = 0x71,   // 原始字节数组 (2B 长度前缀 + 数据)
    Timestamp = 0x81, // 时间戳 (8 字节, Unix 微秒)
};
```

### 4.2 类型大小速查

| 类型 | 字节数 | C++ 类型 |
|------|--------|----------|
| Int8 / UInt8 / Bool / Enum8 / Bitfield8 | 1 | `int8_t` / `uint8_t` |
| Int16 / UInt16 / Enum16 / Bitfield16 | 2 | `int16_t` / `uint16_t` |
| Int32 / UInt32 / Float32 / Bitfield32 | 4 | `int32_t` / `uint32_t` / `float` |
| Timestamp | 8 | `uint64_t` |
| String | 1+N | `QString` |
| Bytes | 2+N | `QByteArray` |

### 4.3 浮点数编码

使用 IEEE 754 单精度 (binary32):

```
位布局: [S:1][E:8][M:23]
小端序存储: [byte0=M[0:7]][byte1=M[8:15]][byte2=M[16:23]+E[0:2]][byte3=S+E[3:7]]

示例: 1.5f → 0x3FC00000 → [0x00, 0x00, 0xC0, 0x3F]
示例: -3.14f → 0xC048F5C3 → [0xC3, 0xF5, 0x48, 0xC0]
```

---

## 6. 流数据帧格式 (Stream Data Frame)

### 5.1 实时变量流 (CMD=0x8001)

```
┌────────────┬──────────────┬──────────────┬─────────────────────────┐
│ timestamp  │ var_count    │ variable[0]  │ variable[1] ... [N-1]   │
│ uint64 (8B)│ uint16 (2B)  │              │                         │
└────────────┴──────────────┴──────────────┴─────────────────────────┘

每个 variable:
┌──────────────┬──────────────┐
│ variable_id  │ value        │
│ uint16 (2B)  │ 4B (固定)    │
└──────────────┴──────────────┘

总载荷长度 = 8 + 2 + var_count * (2 + 4) = 10 + var_count * 6
```

### 5.2 波形数据流 (CMD=0x9001)

```
┌────────────┬──────────────┬──────────────┬─────────────────────────┐
│ timestamp  │ sample_rate  │ sample_count │ samples[0..N-1]         │
│ uint64 (8B)│ uint32 (4B)  │ uint16 (2B)  │ int16[] (2B each)      │
└────────────┴──────────────┴──────────────┴─────────────────────────┘

总载荷长度 = 8 + 4 + 2 + sample_count * 2 = 14 + sample_count * 2
```

---

## 7. 常见消息示例 (Message Examples)

### 6.1 读参数 (CMD_READ_PARAM)

**请求**: 读取参数 ID=0x0020 (PID_Kp)

```
载荷: [param_id:2B]
      [0x20, 0x00]
```

**响应 (成功)**:

```
载荷: [param_id:2B][data_type:1B][value:NB]
      [0x20, 0x00]  [0x21]        [0x00, 0x00, 0xC0, 0x3F]
                                    └──── 1.5f ────┘
```

**响应 (错误)**:

```
载荷: [param_id:2B][error_code:1B]
      [0x20, 0x00]  [0x01]          ← 0x01=参数不存在
```

### 6.2 写参数 (CMD_WRITE_PARAM)

**请求**: 写入参数 ID=0x0020 (PID_Kp), 值=1.5f

```
载荷: [param_id:2B][data_type:1B][value:4B]
      [0x20, 0x00]  [0x21]        [0x00, 0x00, 0xC0, 0x3F]
```

**响应**:

```
载荷: [param_id:2B][status:1B]
      [0x20, 0x00]  [0x00]          ← 0x00=成功
```

### 6.3 批量读参数 (CMD_READ_PARAMS)

**请求**: 读取 ID 0x0010, 0x0020, 0x0030

```
载荷: [count:2B][param_id:2B][param_id:2B][param_id:2B]
      [0x03, 0x00][0x10, 0x00][0x20, 0x00][0x30, 0x00]
```

**响应**:

```
载荷: [count:2B]
      [param_id:2B][data_type:1B][value:NB]  ← 参数0x0010
      [param_id:2B][data_type:1B][value:NB]  ← 参数0x0020
      [param_id:2B][data_type:1B][value:NB]  ← 参数0x0030
```

### 6.4 实时变量流 (CMD=0x8001)

**两路变量: 温度(0x8001)=35.5°C, 电流(0x8002)=12.3A, 时间戳=0x0018FC2A3B000000**

```
载荷:
  [00 00 00 00 3B 2A FC 18]  ← timestamp (uint64, LE)
  [02 00]                     ← var_count = 2
  [01 80] [00 00 0E 42]       ← var_id=0x8001, value=35.5f (0x420E0000)
  [02 80] [CD CC 44 41]       ← var_id=0x8002, value=12.3f (0x4144CCCD)
```

### 6.5 固件升级数据块 (CMD_FW_WRITE_CHUNK)

**请求**: 写入地址 0x08010000, 256 字节数据

```
载荷:
  [offset:4B]          [size:2B]  [data:256B]
  [0x00, 0x00, 0x01, 0x08]  [0x00, 0x01]  [...]
```

**响应**:

```
载荷: [offset:4B][status:1B]
      [0x00, 0x00, 0x01, 0x08]  [0x00]  ← 0x00=成功
```

### 6.6 紧急停止 (CMD_MOTOR_EMERGENCY_STOP)

**请求** (空载荷):

```
载荷: (空)
```

**响应**:

```
载荷: [status:1B]
      [0x00]  ← 0x00=已停止, 0x01=已在停止状态
```

---

## 8. 变量注册表 (Variable Registry)

### 7.1 预定义变量 ID

| 变量 ID | 名称 | 类型 | 单位 | 缩放 | 描述 |
|---------|------|------|------|------|------|
| 0x8001 | `motor.temperature` | Float32 | °C | 1.0 | 电机温度 |
| 0x8002 | `motor.current_a` | Float32 | A | 1.0 | A 相电流 |
| 0x8003 | `motor.current_b` | Float32 | A | 1.0 | B 相电流 |
| 0x8004 | `motor.current_c` | Float32 | A | 1.0 | C 相电流 |
| 0x8005 | `motor.voltage_bus` | Float32 | V | 1.0 | 母线电压 |
| 0x8006 | `motor.speed_rpm` | Float32 | rpm | 1.0 | 转速 |
| 0x8007 | `motor.position` | Float32 | deg | 1.0 | 位置 |
| 0x8008 | `motor.torque` | Float32 | Nm | 1.0 | 转矩 |
| 0x8009 | `motor.power` | Float32 | W | 1.0 | 功率 |
| 0x800A | `motor.efficiency` | Float32 | % | 1.0 | 效率 |
| 0x8010 | `driver.temperature` | Float32 | °C | 1.0 | 驱动器温度 |
| 0x8011 | `driver.voltage_input` | Float32 | V | 1.0 | 输入电压 |
| 0x8020 | `controller.error_code` | Bitfield32 | — | — | 错误码 |
| 0x8021 | `controller.state` | Enum8 | — | — | 运行状态 |
| 0x8022 | `controller.control_mode` | Enum8 | — | — | 控制模式 |

### 7.2 注册表结构

```cpp
struct VariableDefinition {
    uint16_t id;
    DataType type;
    QString name;          // 如 "motor.temperature"
    QString displayName;   // 如 "电机温度"
    QString unit;          // 如 "°C"
    float scale;           // 缩放因子, raw * scale + offset = 物理值
    float offset;
    float minValue;        // 显示范围下限
    float maxValue;        // 显示范围上限
    QString group;         // 分组: "motor" / "driver" / "controller"
    bool isWaveform;       // 是否为高频波形变量
};

class VariableRegistry {
public:
    void registerVariable(const VariableDefinition& def);
    const VariableDefinition* lookup(uint16_t id) const;
    const VariableDefinition* lookup(const QString& name) const;
    QVector<const VariableDefinition*> variablesByGroup(const QString& group) const;

private:
    QHash<uint16_t, VariableDefinition> m_byId;
    QHash<QString, uint16_t> m_byName;
};
```

---

## 9. 版本兼容规则 (Version Compatibility)

### 8.1 消息格式演进规则

| 变更类型 | 兼容性 | 规则 |
|----------|--------|------|
| 新增命令码 | 向前兼容 | 旧版本返回 `CMD_UNKNOWN` (0xFFFF) |
| 载荷末尾追加字段 | 向前兼容 | 旧版本忽略未知尾部字段 |
| 载荷中间插入字段 | **不兼容** | 需要协议版本升级 |
| 修改字段类型/大小 | **不兼容** | 需要协议版本升级 |
| 删除字段 | **不兼容** | 需要协议版本升级 |
| 变更字节序 | **不兼容** | 全局协议版本升级 |

### 8.2 兼容性处理

```cpp
// 发送方: 附带版本号
Frame frame;
frame.setPayload(serializeForVersion(negotiatedVersion));

// 接收方: 按版本解析
void handleResponse(const Frame& frame) {
    uint8_t version = m_negotiatedVersion;
    if (version >= 2) {
        // 解析 v2+ 新增字段
    }
    // 解析通用字段
}
```

### 8.3 未知命令处理

```cpp
// 下位机收到未知命令时:
// 返回 CMD=0xFFFF (CMD_UNKNOWN)
// 载荷: [original_cmd:2B]
//
// 上位机收到 CMD_UNKNOWN:
// 1. 记录日志 "命令 0x%04X 不被设备支持"
// 2. 如果该命令为关键功能，提示用户升级固件
// 3. 如果该命令为可选功能，静默降级
```

---

## 10. API接口规划 (API Interface Planning)

### 10.1 MessageSerializer 接口

```cpp
// messageserializer.h
class MessageSerializer {
public:
    // ── 参数读写 ──

    /// 序列化读参数请求: [param_id:2B]
    static QByteArray serializeReadParam(uint16_t paramId);

    /// 反序列化读参数响应: [param_id:2B][data_type:1B][value:NB]
    static VariableValue deserializeReadParamResponse(const QByteArray& payload);

    /// 序列化写参数请求: [param_id:2B][data_type:1B][value:NB]
    static QByteArray serializeWriteParam(uint16_t paramId, const VariableValue& value);

    /// 反序列化写参数响应: [param_id:2B][status:1B]
    static bool deserializeWriteParamResponse(const QByteArray& payload);

    // ── 批量参数 ──

    /// 序列化批量读参数请求: [count:2B][param_id:2B]...
    static QByteArray serializeReadParams(const QVector<uint16_t>& paramIds);

    /// 反序列化批量读参数响应: [count:2B][param_id:2B][data_type:1B][value:NB]...
    static QVector<VariableValue> deserializeReadParamsResponse(const QByteArray& payload);

    // ── 流数据 ──

    /// 反序列化实时变量流帧: [timestamp:8B][var_count:2B][var_id:2B][value:4B]...
    static QVector<VariableValue> deserializeStreamData(const QByteArray& payload,
                                                         const VariableRegistry& registry);

    /// 反序列化波形数据流帧: [timestamp:8B][sample_rate:4B][sample_count:2B][samples:2B]...
    static WaveformData deserializeWaveformData(const QByteArray& payload);

    // ── 固件升级 ──

    /// 序列化固件写入块: [offset:4B][size:2B][data:NB]
    static QByteArray serializeFirmwareChunk(uint32_t offset, const QByteArray& data);

    /// 反序列化固件写入响应: [offset:4B][status:1B]
    static bool deserializeFirmwareChunkResponse(const QByteArray& payload, uint32_t& offset);

    // ── 通用 ──

    /// 反序列化错误响应: [original_cmd:2B][error_code:1B]
    static ErrorCode deserializeErrorResponse(const QByteArray& payload, uint16_t& originalCmd);

    /// 验证载荷长度是否与预期一致
    static bool validatePayloadSize(const QByteArray& payload, size_t expectedSize);
};
```

### 10.2 VariableRegistry 接口

```cpp
// variableregistry.h
class VariableRegistry {
public:
    static VariableRegistry* instance();

    // 注册变量
    void registerVariable(const VariableDefinition& def);

    // 批量注册 (从 JSON 加载)
    bool loadFromJson(const QString& jsonFilePath);
    bool loadFromJson(const QJsonArray& array);

    // 查询
    const VariableDefinition* lookup(uint16_t id) const;
    const VariableDefinition* lookup(const QString& name) const;

    // 按分组查询
    QVector<const VariableDefinition*> variablesByGroup(const QString& group) const;

    // 所有变量
    QVector<const VariableDefinition*> allVariables() const;

    // 变量数量
    size_t count() const;

    // 检查 ID 是否存在
    bool contains(uint16_t id) const;

private:
    QHash<uint16_t, VariableDefinition> m_byId;
    QHash<QString, uint16_t> m_byName;
};
```

### 10.3 使用示例

```cpp
// 初始化变量注册表
auto& registry = *VariableRegistry::instance();
registry.loadFromJson(":/config/variables.json");

// 发送读参数命令
auto payload = MessageSerializer::serializeReadParam(0x0020);
auto response = commManager->sendCommand("motor1", 0x1001, payload).result();

// 解析响应
auto varValue = MessageSerializer::deserializeReadParamResponse(response.payload);
qDebug() << "Param" << varValue.variableId << "=" << varValue.toFloat();

// 解析流数据
void onStreamFrame(const Frame& frame) {
    auto variables = MessageSerializer::deserializeStreamData(frame.payload, registry);
    for (const auto& var : variables) {
        auto* def = registry.lookup(var.variableId);
        if (def) {
            qDebug() << def->displayName << "=" << var.toFloat() << def->unit;
        }
    }
}
```

---

## 11. 后续实现注意事项 (Implementation Notes)

| 类别 | 注意事项 |
|------|----------|
| **命令码管理** | 使用 `enum class CommandCode : uint16_t` 集中定义，禁止硬编码数值；新增命令码必须在本文档中登记 |
| **序列化/反序列化** | 为每种消息类型编写专用的 `serialize()` / `deserialize()` 函数，使用 `QDataStream` 并设置为 `LittleEndian` |
| **类型安全** | 反序列化时校验 `data_type` 与预期一致，类型不匹配时返回错误而非尝试转换 |
| **边界检查** | 所有反序列化函数必须检查缓冲区长度，防止越界读取导致崩溃 |
| **变量注册表** | 变量定义从 JSON 配置文件加载，支持运行时热更新（不重启程序） |
| **流数据解析** | 流数据帧中的 `variable_id` 必须在 `VariableRegistry` 中存在，否则跳过该变量并记录警告 |
| **浮点特殊值** | 支持 NaN、±Inf 的传输与显示（Float32 的 IEEE 754 特殊编码），但下位机不应发送 NaN 作为有效数据 |
| **单元测试** | 每种消息类型必须包含: 正常序列化往返、边界值（max/min/zero）、缓冲区截断、类型不匹配 |
| **性能** | 流数据解析路径禁止内存分配，使用预分配缓冲区；`QDataStream` 在热路径中替换为直接指针操作 |
| **文档同步** | 每次新增命令码或变量时，必须同步更新本文档，作为下位机与上位机团队的接口契约 |

---

## 附录 A: 错误码定义

```cpp
enum class ErrorCode : uint8_t {
    OK              = 0x00,
    ParamNotFound   = 0x01,
    ParamReadOnly   = 0x02,
    ParamOutOfRange = 0x03,
    MotorDisabled   = 0x10,
    MotorFault      = 0x11,
    EmergencyStop   = 0x12,
    Busy            = 0x20,
    FlashError      = 0x30,
    FwVerifyFailed  = 0x31,
    FwSizeTooLarge  = 0x32,
    UnknownCommand  = 0xFE,
    InternalError   = 0xFF,
};
```

## 附录 B: 运行状态枚举

```cpp
enum class MotorState : uint8_t {
    Idle        = 0x00,
    Ready       = 0x01,
    Running     = 0x02,
    Stopping    = 0x03,
    Fault       = 0x04,
    Emergency   = 0x05,
    Calibrating = 0x06,
    Updating    = 0x07,
};

enum class ControlMode : uint8_t {
    Position    = 0x00,
    Speed       = 0x01,
    Torque      = 0x02,
    Current     = 0x03,
};
```

## 附录 C: QDataStream 序列化辅助

```cpp
// 统一使用小端序
QDataStream& operator<<(QDataStream& s, const Frame& f) {
    s.setByteOrder(QDataStream::LittleEndian);
    s << f.stx << f.len << f.seq << f.cmd;
    s.writeRawData(f.payload.constData(), f.payload.size());
    s << f.crc16 << f.etx;
    return s;
}

// 便捷宏
#define SERIALIZE_LE(stream, value) \
    do { stream.setByteOrder(QDataStream::LittleEndian); stream << value; } while(0)
```