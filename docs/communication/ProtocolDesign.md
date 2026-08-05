# 协议设计 (Protocol Design)

> 版本: 1.0 | 状态: 设计阶段 | 作者: 系统架构组

---

## 1. 目标 (Goal)

定义电机监控上位机与下位机之间的**二进制帧协议**，确保在工业噪声环境中实现：

- **帧同步可靠**：在任何字节流中准确定位帧边界，容忍字节丢失与插入。
- **数据完整性**：通过 CRC16 校验检测传输错误，误判率可控。
- **高效编码**：使用 COBS（Consistent Overhead Byte Stuffing）替代传统转义字符，开销固定且确定。
- **大包支持**：支持大于 MTU 的载荷通过分片/重组传输。
- **版本协商**：协议版本可在握手阶段协商，向前兼容。
- **双模式区分**：命令帧与流数据帧通过命令码范围区分，路由到不同处理路径。

---

## 2. 设计原则 (Design Principles)

| 原则 | 说明 |
|------|------|
| **确定性边界** | 使用 STX/ETX 哨兵字节 + COBS 编码，帧边界在任何数据流中可确定 |
| **最小开销** | 帧头固定 8 字节 + 尾部 4 字节，COBS 开销 ≤ ⌈N/254⌉（N 为载荷长度） |
| **状态机驱动** | FrameCodec 使用明确的有限状态机，每个状态转换可追踪 |
| **错误即停** | 在任何状态检测到非法字节时，立即重置到 Idle，不尝试恢复（避免错误传播） |
| **校验优先** | 先校验 CRC 再解析载荷，防止对损坏数据的误操作 |
| **大端元数据** | 帧头字段（LEN、SEQ、CMD）使用小端序（与下位机 ARM Cortex-M 一致） |

---

## 3. 类/模块关系 (Class/Module Relationships)

### 3.1 模块关系图

```
┌──────────────────────────────────────────────────────────┐
│                      Protocol Layer                       │
│                                                          │
│  ┌──────────────┐    ┌──────────────┐    ┌────────────┐  │
│  │  FrameCodec  │───▶│ COBSEncoder  │    │  CRC16     │  │
│  │  (状态机)     │    │  (编码/解码)  │    │  (查表法)   │  │
│  │              │    └──────────────┘    └────────────┘  │
│  │  - feed()    │                                        │
│  │  - encode()  │    ┌──────────────┐                    │
│  │  - reset()   │───▶│ Fragmenter   │                    │
│  └──────────────┘    │  (分片/重组)  │                    │
│                      └──────────────┘                    │
│                                                          │
│  输入: 字节流 (ITransport)                                │
│  输出: Frame 对象 (CommandQueue / StreamParser)           │
└──────────────────────────────────────────────────────────┘
```

### 3.2 核心类依赖

| 类 | 依赖 | 职责 |
|----|------|------|
| `FrameCodec` | `COBSEncoder`, `CRC16` | 字节流 → Frame 的完整状态机，编解码总控 |
| `COBSEncoder` | 无 | 纯函数：COBS 编码/解码，无状态 |
| `CRC16` | 无 | 纯函数：CRC16-CCITT 查表计算 |
| `Fragmenter` | `FrameCodec` | 大包分片封装与重组，管理分片上下文 |
| `Frame` | 无 | 数据结构：STX/LEN/SEQ/CMD/PAYLOAD/CRC/ETX |

### 3.3 Frame 数据结构

```cpp
struct Frame {
    static constexpr uint16_t STX_VALUE = 0xAA55;
    static constexpr uint16_t ETX_VALUE = 0x55AA;
    static constexpr size_t   HEADER_SIZE = 8;   // STX+LEN+SEQ+CMD
    static constexpr size_t   TRAILER_SIZE = 4;  // CRC+ETX
    static constexpr size_t   MAX_PAYLOAD = 1024;

    uint16_t   stx = STX_VALUE;
    uint16_t   len = 0;
    uint16_t   seq = 0;
    uint16_t   cmd = 0;
    QByteArray payload;
    uint16_t   crc16 = 0;
    uint16_t   etx = ETX_VALUE;

    bool isValid() const;
    bool isCommand() const;  // cmd < 0x8000
    bool isStream() const;   // cmd >= 0x8000
    size_t totalSize() const { return HEADER_SIZE + len + TRAILER_SIZE; }
};
```

---

## 4. 帧结构定义 (Frame Structure)

### 3.1 整体帧格式

```
┌──────┬──────┬──────┬──────┬──────┬──────┬───────────┬──────┬──────┬──────┬──────┐
│ STX1 │ STX2 │ LEN_L│ LEN_H│ SEQ_L│ SEQ_H│ CMD_L│CMD_H│ PAYLOAD  │CRC_L│CRC_H│ ETX1 │ ETX2 │
│ 0xAA │ 0x55 │      │      │      │      │      │     │ (0..N B) │     │     │ 0x55 │ 0xAA │
└──────┴──────┴──────┴──────┴──────┴──────┴──────┴─────┴───────────┴─────┴─────┴──────┴──────┘
  2B      2B      2B      2B      2B      2B      2B       NB        2B      2B      2B
  └─ STX ─┘ └──── LEN ────┘ └──── SEQ ────┘ └──── CMD ────┘          └─ CRC ─┘ └─ ETX ─┘
```

### 3.2 字段说明

| 字段 | 偏移 | 长度 | 类型 | 描述 |
|------|------|------|------|------|
| STX | 0 | 2 | uint16 | 帧起始标记: `0xAA55` (小端字节序: `[0xAA, 0x55]`) |
| LEN | 2 | 2 | uint16 | 载荷长度 (0-65535 字节)，不含 STX/CRC/ETX |
| SEQ | 4 | 2 | uint16 | 序列号，自增，用于命令-响应匹配 |
| CMD | 6 | 2 | uint16 | 命令码 / 流数据标识 |
| PAYLOAD | 8 | LEN | bytes | 载荷数据 |
| CRC16 | 8+LEN | 2 | uint16 | CRC16-CCITT 校验值，覆盖范围: [LEN_L .. PAYLOAD_END] |
| ETX | 10+LEN | 2 | uint16 | 帧结束标记: `0x55AA` (小端字节序: `[0x55, 0xAA]`) |

### 3.3 帧长度计算

```
总帧长 = 8 (帧头) + LEN (载荷) + 4 (CRC+ETX) = 12 + LEN
最小帧长 = 12 (LEN=0, 空载荷)
最大帧长 = 12 + 65535 = 65547 字节
```

### 3.4 字节序

**全部多字节字段使用小端序 (Little-Endian)**，与 ARM Cortex-M 系列 MCU 原生字节序一致。

```
示例: SEQ = 0x1234
在帧中: [0x34, 0x12]
```

---

## 5. COBS 编码 (Consistent Overhead Byte Stuffing)

### 4.1 为什么使用 COBS

传统帧同步使用转义字符（如 SLIP 协议的 `0xDB 0xDC` 替换 `0xC0`），但存在以下问题：

- **开销不确定**：最坏情况下每个字节都需要转义，开销达 100%。
- **实现复杂**：需要逐字节检查并替换，状态机复杂。

COBS 的优点是：

- **开销固定且确定**：最多 ⌈N/254⌉ 字节，N 为原始数据长度。
- **解码简单**：单次扫描即可完成，适合嵌入式 MCU 和上位机。
- **无特殊字节冲突**：编码后不含 `0x00`，可用 `0x00` 作为帧分隔符。

### 4.2 COBS 编码流程

```
算法: COBS Encode

输入: 原始数据 input[0..N-1]
输出: 编码数据 output[0..M-1]

1. 初始化 code_ptr = 0, output[0] = 0x01 (下一个 0x00 的位置)
2. 遍历 input[i]:
   a. 如果 input[i] == 0x00:
      - output[code_ptr] = i - last_zero + 1
      - code_ptr = i + 1
      - output[code_ptr] = 0x01
   b. 否则:
      output[i+1] = input[i]
3. 最后: output[code_ptr] = 0x01 (或 0x00, 表示结束)
   如果 N < 254: 在 output 末尾追加单个 0x00 作为分隔符
```

### 4.3 COBS 解码流程

```
算法: COBS Decode

输入: 编码数据 input[0..M-1]
输出: 原始数据 output[0..N-1]

1. code = input[0]; read_ptr = 1; write_ptr = 0
2. while read_ptr < M:
   a. 如果 code == 0x00: 结束 (不应在帧中间出现)
   b. 取 next_byte = input[read_ptr++]; code--
   c. 如果 code > 0: output[write_ptr++] = next_byte
   d. 如果 code == 0: output[write_ptr++] = 0x00; code = next_byte
```

### 4.4 COBS 开销分析

| 原始载荷长度 | COBS 开销 | 开销百分比 |
|-------------|-----------|-----------|
| 1-253 字节 | 1 字节 | ≤ 0.4% |
| 254-507 字节 | 2 字节 | ≤ 0.4% |
| 1024 字节 | 5 字节 | ≈ 0.49% |
| 4096 字节 | 17 字节 | ≈ 0.41% |
| 65535 字节 | 259 字节 | ≈ 0.40% |

**结论**: 对于电机监控场景（典型帧载荷 64-1024 字节），COBS 开销可忽略不计。

---

## 6. FrameCodec 状态机 (FrameCodec State Machine)

### 5.1 状态转换图

```
                         ┌──────────────────────────────────────┐
                         │                                      │
                         ▼                                      │
                    ┌─────────┐                                  │
          ────────▶│  IDLE   │◀───────────────────────┐          │
          │        └────┬────┘                        │          │
          │        0xAA │                             │          │
          │             ▼                             │          │
          │        ┌─────────┐                        │          │
          │        │  STX1   │──(非0x55)──────────────┼──────────┤
          │        └────┬────┘                        │          │
          │        0x55 │                             │          │
          │             ▼                             │          │
          │        ┌─────────┐                        │          │
          │        │  STX2   │                        │          │
          │        └────┬────┘                        │          │
          │             │ 读取 2B LEN                  │          │
          │             ▼                             │          │
          │        ┌─────────┐                        │          │
          │        │  LEN    │                        │          │
          │        └────┬────┘                        │          │
          │             │ 读取 2B SEQ + 2B CMD        │          │
          │             ▼                             │          │
          │        ┌─────────┐                        │          │
          │        │ HEADER  │                        │          │
          │        └────┬────┘                        │          │
          │             │ LEN > 0? 读取 LEN 字节       │          │
          │             ▼                             │          │
          │        ┌─────────┐                        │          │
          │        │ PAYLOAD │                        │          │
          │        └────┬────┘                        │          │
          │             │ 读取 2B CRC                  │          │
          │             ▼                             │          │
          │        ┌─────────┐     CRC 错误            │          │
          │        │  CRC    │────────────▶ ERROR ────┘          │
          │        └────┬────┘                                   │
          │             │ CRC 正确, 读取 2B ETX                   │
          │             ▼                             ＜── ERROR ┤
          │        ┌─────────┐                        │          │
          │        │  ETX    │──(非0x55AA)────────────┘          │
          │        └────┬────┘                                   │
          │             │ 0x55AA                                 │
          │             ▼                                        │
          │        ┌─────────┐                                   │
          │        │  DONE   │──▶ frameReady(Frame)              │
          │        └─────────┘                                   │
          │             │                                        │
          └─────────────┘ (自动回到 IDLE)
```

### 5.2 状态机实现骨架

```cpp
void FrameCodec::feed(uint8_t byte) {
    switch (m_state) {
    case State::Idle:
        if (byte == 0xAA) {
            m_state = State::STX1;
            m_buffer.append(byte);
        }
        // 非0xAA静默丢弃
        break;

    case State::STX1:
        if (byte == 0x55) {
            m_state = State::STX2;
            m_buffer.append(byte);
        } else {
            reset();  // 不是0x55，回到Idle
        }
        break;

    case State::STX2:
        m_buffer.append(byte);
        if (m_buffer.size() == 4) {  // STX(2) + LEN(2)
            m_expectedLen = qFromLittleEndian<uint16_t>(m_buffer.data() + 2);
            m_state = (m_expectedLen <= MAX_PAYLOAD_LEN) ? State::Header : State::Idle;
            if (m_state == State::Idle) reset();
        }
        break;

    case State::Header:
        m_buffer.append(byte);
        if (m_buffer.size() == 8) {  // STX+LEN+SEQ+CMD
            m_state = (m_expectedLen > 0) ? State::Payload : State::CRC;
        }
        break;

    case State::Payload:
        m_buffer.append(byte);
        if (m_buffer.size() >= 8u + m_expectedLen) {
            m_state = State::CRC;
        }
        break;

    case State::CRC:
        m_buffer.append(byte);
        if (m_buffer.size() >= 10u + m_expectedLen) {
            uint16_t receivedCrc = qFromLittleEndian<uint16_t>(
                m_buffer.data() + 8 + m_expectedLen);
            uint16_t computedCrc = crc16_ccitt(
                m_buffer.data() + 2, 6 + m_expectedLen);  // LEN..PAYLOAD_END
            if (receivedCrc == computedCrc) {
                m_state = State::ETX;
            } else {
                emit frameError(FrameError::CRCMismatch, 
                    QString("expected=0x%1, actual=0x%2")
                        .arg(computedCrc, 4, 16, QChar('0'))
                        .arg(receivedCrc, 4, 16, QChar('0')));
                reset();
            }
        }
        break;

    case State::ETX:
        m_buffer.append(byte);
        if (m_buffer.size() >= 12u + m_expectedLen) {
            uint16_t etx = qFromLittleEndian<uint16_t>(
                m_buffer.data() + 10 + m_expectedLen);
            if (etx == 0x55AA) {
                Frame frame = buildFrame();
                emit frameReady(frame);
            } else {
                emit frameError(FrameError::InvalidETX, "");
            }
            reset();
        }
        break;
    }
}
```

---

## 7. 命令帧 vs 流数据帧 (CMD Range)

### 6.1 地址空间划分

```
CMD 16-bit 地址空间: 0x0000 - 0xFFFF
                      │
          ┌───────────┴───────────┐
          │                       │
    0x0000 - 0x7FFF          0x8000 - 0xFFFF
    命令帧 (Command)          流数据帧 (Stream)
          │                       │
    ┌─────┴──────┐          ┌─────┴──────┐
0x0000-0x0FFF  0x1000-0x7FFF  0x8000-0x8FFF  0x9000-0xFFFF
 系统命令      应用命令        实时变量流      波形数据流
```

### 6.2 路由规则

```cpp
FrameType classifyFrame(uint16_t cmd) {
    if (cmd & 0x8000) {
        return FrameType::Stream;   // 进入 StreamParser
    } else {
        return FrameType::Command;  // 进入 CommandDispatcher
    }
}
```

---

## 8. CRC16-CCITT 校验

### 7.1 算法参数

| 参数 | 值 |
|------|-----|
| 多项式 | 0x1021 (x^16 + x^12 + x^5 + 1) |
| 初始值 | 0xFFFF |
| 输入反转 | 否 |
| 输出反转 | 否 |
| 异或输出 | 0x0000 |
| 算法名 | CRC-16/CCITT-FALSE |

### 7.2 查表实现

```cpp
// 预计算查找表
static const uint16_t crc16_table[256] = { /* ... 预计算值 */ };

uint16_t crc16_ccitt(const uint8_t* data, size_t len) {
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < len; i++) {
        crc = (crc << 8) ^ crc16_table[((crc >> 8) ^ data[i]) & 0xFF];
    }
    return crc;
}
```

### 7.3 误判率分析

CRC16-CCITT 的数学特性：

- **汉明距离**: 对长度 ≤ 32767 位的消息，汉明距离 = 4（可检测所有 1/2/3 位错误）
- **突发错误检测**: 可检测所有长度 ≤ 16 位的突发错误
- **随机错误漏检率**: 1 / 2^16 ≈ **0.0015%** (每 65536 个错误帧漏检 1 个)

对于电机监控场景（假设 BER = 10^-6，帧长 256 字节）：

```
帧错误概率 P_frame_error ≈ 256 * 8 * 10^-6 ≈ 0.2%
漏检概率 ≈ 0.2% * 0.0015% ≈ 3 * 10^-8
```

**结论**: 在典型工业环境下，CRC16 漏检率可忽略不计。

---

## 9. 大包分片与重组 (Fragmentation & Reassembly)

### 8.1 分片策略

当载荷超过 `MAX_PAYLOAD_PER_FRAME`（默认 1024 字节）时，自动分片：

```
原始载荷: [ chunk0 | chunk1 | chunk2 | ... | chunkN-1 ]

每个分片帧:
  CMD = 0x00FF (分片传输命令)
  PAYLOAD = [frag_id:2B][total_frags:2B][offset:4B][chunk_data:NB]
```

### 8.2 分片帧格式

| 字段 | 偏移 | 长度 | 描述 |
|------|------|------|------|
| frag_id | 0 | 2 | 分片 ID (0..total-1) |
| total_frags | 2 | 2 | 总分片数 |
| offset | 4 | 4 | 在原始载荷中的字节偏移 |
| chunk_data | 8 | NB | 分片数据 (最大 1016 字节) |

### 8.3 重组逻辑

```cpp
class FragmentReassembler {
    struct ReassemblyContext {
        uint16_t seqId;
        uint16_t totalFrags;
        QVector<QByteArray> fragments;  // 按 frag_id 索引
        QSet<uint16_t> receivedFrags;
        QElapsedTimer timer;
        std::chrono::milliseconds timeout{5000};
    };

    QHash<uint16_t, ReassemblyContext> m_contexts;

    std::optional<QByteArray> feedFragment(const Frame& frame);
    // 返回完整载荷 或 nullopt (等待更多分片)

    void cleanupExpired();
};
```

---

## 10. 错误处理 (Error Handling)

### 9.1 错误分类与处理策略

| 错误类型 | 检测方式 | 处理策略 |
|----------|----------|----------|
| CRC 不匹配 | 收到 CRC 与实际计算不符 | 丢弃帧，`frameError(CRCMismatch)`，统计 `framesDropped` |
| 帧超时 | 等待 ETX 超时 (默认 100ms 帧间超时) | 重置状态机，`frameError(Timeout)` |
| 缓冲区溢出 | LEN > MAX_PAYLOAD_LEN | 重置状态机，`frameError(BufferOverflow)` |
| 非法 STX/ETX | 未在预期位置收到 STX/ETX | 重置状态机，搜索下一个 STX |
| COBS 解码错误 | 解码时遇到非法序列 | 丢弃帧，`frameError(COBSDecodeError)` |
| 分片超时 | 重组上下文超时未收齐 | 释放重组上下文，通知上层 |

### 9.2 错误恢复

```
原则: 快速失败 → 快速恢复 (Fail Fast → Recover Fast)

- 任何错误立即重置状态机到 Idle
- 不清除接收缓冲区中后续有效数据
- 错误计数达到阈值 (如 100次/分钟) 时，触发告警并建议检查硬件
```

---

## 11. 协议版本协商 (Version Negotiation)

### 10.1 握手流程

```
上位机                                    下位机
  │                                         │
  │──── CMD=0x0001, PAYLOAD=[ver=2,         │
  │      features=0x000F] ─────────────────▶│ 版本查询
  │                                         │
  │◀──── CMD=0x0001, PAYLOAD=[ver=2,        │
  │      features=0x0007, status=OK] ───────│ 版本响应
  │                                         │
  │ 比较 features，取交集                     │
  │ 决定使用协议版本: min(上位机ver, 下位机ver) │
  │                                         │
  │──── CMD=0x0002, PAYLOAD=[ver=2] ───────▶│ 确认版本
  │                                         │
  │◀──── CMD=0x0002, PAYLOAD=[status=OK] ───│ 确认响应
  │                                         │
  │ 后续通信使用协商后的版本                    │
```

### 10.2 版本兼容规则

- 主版本号相同 → 兼容
- 主版本号不同 → 不兼容，拒绝连接
- 次版本号不同 → 兼容，使用较低版本
- Features 位掩码取交集，双方都支持的功能才启用

---

## 12. API接口规划 (API Interface Planning)

### 12.1 FrameCodec 接口

```cpp
// framecodec.h
class FrameCodec : public QObject {
    Q_OBJECT
public:
    enum State { Idle, STX1, STX2, LenLow, LenHigh, Payload, CrcLow, CrcHigh, ETX1, ETX2 };

    // 喂入字节流，状态机驱动
    void feed(uint8_t byte);
    void feed(const QByteArray& data);

    // 编码帧为字节流 (静态方法，线程安全)
    static QByteArray encode(uint16_t cmd, const QByteArray& payload);
    static QByteArray encodeFrame(const Frame& frame);

    // 重置状态机
    void reset();
    State currentState() const;

signals:
    void frameReady(const Frame& frame);
    void frameError(FrameError error, const QString& detail);
    void stateChanged(State oldState, State newState);
};
```

### 12.2 COBSEncoder 接口

```cpp
// cobsencoder.h
class COBSEncoder {
public:
    // 编码: 输入不含 0x00 的数据，输出 COBS 编码数据
    // 输出缓冲区需 >= input.size() + input.size()/254 + 2
    static size_t encode(const uint8_t* input, size_t inputLen,
                         uint8_t* output, size_t outputCapacity);

    // 解码: 输入 COBS 编码数据，输出原始数据
    // 返回实际解码的字节数，0 表示解码失败
    static size_t decode(const uint8_t* input, size_t inputLen,
                         uint8_t* output, size_t outputCapacity);

    // 便捷方法
    static QByteArray encode(const QByteArray& data);
    static QByteArray decode(const QByteArray& data);

    // 最大编码后长度
    static size_t maxEncodedSize(size_t rawSize);
};
```

### 12.3 CRC16 接口

```cpp
// crc16.h
class CRC16 {
public:
    // 计算 CRC16-CCITT
    static uint16_t compute(const uint8_t* data, size_t len);

    // 便捷方法
    static uint16_t compute(const QByteArray& data);

    // 校验
    static bool verify(const uint8_t* data, size_t len, uint16_t expectedCrc);
};
```

### 12.4 Fragmenter 接口

```cpp
// fragmenter.h
class Fragmenter {
public:
    static constexpr size_t MAX_CHUNK_SIZE = 1016; // 1024 - 8B 分片头

    // 分片: 大载荷 → 多个分片帧
    static QVector<Frame> fragment(uint16_t seqId, const QByteArray& payload);

    // 重组: 接收分片帧，返回完整载荷 (或 nullopt 表示等待更多分片)
    std::optional<QByteArray> reassemble(const Frame& fragFrame);

    // 清理超时上下文
    void cleanupExpired(std::chrono::milliseconds timeout = std::chrono::milliseconds(5000));

    // 当前活跃的重组上下文数
    size_t activeContexts() const;

private:
    struct ReassemblyContext {
        uint16_t seqId;
        uint16_t totalFrags;
        QVector<QByteArray> fragments;
        QSet<uint16_t> receivedFrags;
        QElapsedTimer timer;
    };
    QHash<uint16_t, ReassemblyContext> m_contexts;
};
```

### 12.5 使用示例

```cpp
// 发送端: 编码命令帧
QByteArray payload = serializeReadParam(0x0020);
QByteArray rawFrame = FrameCodec::encode(0x1001, payload);

// 如果载荷过大，先分片
if (payload.size() > Frame::MAX_PAYLOAD) {
    auto fragments = Fragmenter::fragment(seqId, payload);
    for (auto& frag : fragments) {
        QByteArray raw = FrameCodec::encodeFrame(frag);
        transport->send(raw);
    }
} else {
    transport->send(rawFrame);
}

// 接收端: 解码
FrameCodec codec;
QObject::connect(&codec, &FrameCodec::frameReady, [](const Frame& frame) {
    if (frame.cmd == 0x00FF) {
        // 分片帧，交给 Fragmenter 重组
        auto result = fragmenter.reassemble(frame);
        if (result.has_value()) {
            handleCompletePayload(result.value());
        }
    } else {
        handleFrame(frame);
    }
});

// 字节到达时
codec.feed(receivedBytes);
```

---

## 13. 后续实现注意事项 (Implementation Notes)

| 类别 | 注意事项 |
|------|----------|
| **状态机实现** | 使用 `switch-case` 而非虚函数表，每个状态转换 < 10 CPU 周期；在 `feed()` 方法中避免内存分配 |
| **COBS 实现** | 编码输出缓冲区大小 = 输入大小 + ⌈输入大小/254⌉ + 1；使用 SIMD 优化（可选） |
| **CRC 计算** | 使用查表法（256 项 × 2 字节 = 512 字节），在构造函数中预计算；支持硬件 CRC（ARM Cortex-M 有 CRC 外设） |
| **分片超时** | 分片重组超时默认 5 秒，可通过 `QSettings` 配置；超时后清理重组上下文，避免内存泄漏 |
| **帧间超时** | 帧间超时时间 = 最大帧长 / 波特率 × 10（安全系数），典型值 100ms |
| **单元测试** | 必须覆盖: 正常帧、边界帧（LEN=0, LEN=MAX）、损坏帧（CRC错误、STX错误、截断帧）、COBS 编解码往返、分片/重组完整流程 |
| **性能测试** | 在 921600 bps 串口下，FrameCodec 吞吐量应 > 10000 帧/秒 |
| **日志** | 帧错误日志包含原始字节（hex dump），便于现场调试 |
| **协议文档** | 协议变更时同步更新本文档，版本号递增 |

---

## 附录 A: 帧示例

### 空命令帧 (心跳)

```
STX     LEN   SEQ   CMD   CRC16  ETX
AA 55   00 00 01 00 00 01 4B 0A  55 AA
```

### 读参数命令 (读取电机温度, 参数ID=0x0010)

```
STX     LEN   SEQ   CMD   PAYLOAD   CRC16  ETX
AA 55   02 00 02 00 10 00 10 00     E2 91  55 AA
                                └──参数ID──┘
```

### 写参数命令 (设置PID_Kp=1.5f, 参数ID=0x0020)

```
STX     LEN   SEQ   CMD   PAYLOAD           CRC16  ETX
AA 55   06 00 03 00 20 00 20 00 00 00 C0 3F  XX XX  55 AA
                                └──ID──┘ └─1.5f──┘
```

### 流数据帧 (两路变量: 温度=35.5, 电流=12.3)

```
STX     LEN    SEQ   CMD   PAYLOAD                              CRC16  ETX
AA 55   16 00  04 00 01 80  [timestamp:8B][count:2B=02]
                            [id=0x8001:2B][val=35.5:4B]
                            [id=0x8002:2B][val=12.3:4B]          XX XX  55 AA
```

## 附录 B: CRC16 查找表

```cpp
static const uint16_t crc16_table[256] = {
    0x0000, 0x1021, 0x2042, 0x3063, 0x4084, 0x50A5, 0x60C6, 0x70E7,
    0x8108, 0x9129, 0xA14A, 0xB16B, 0xC18C, 0xD1AD, 0xE1CE, 0xF1EF,
    0x1231, 0x0210, 0x3273, 0x2252, 0x52B5, 0x4294, 0x72F7, 0x62D6,
    0x9339, 0x8318, 0xB37B, 0xA35A, 0xD3BD, 0xC39C, 0xF3FF, 0xE3DE,
    0x2462, 0x3443, 0x0420, 0x1401, 0x64E6, 0x74C7, 0x44A4, 0x5485,
    0xA56A, 0xB54B, 0x8528, 0x9509, 0xE5EE, 0xF5CF, 0xC5AC, 0xD58D,
    0x3653, 0x2672, 0x1611, 0x0630, 0x76D7, 0x66F6, 0x5695, 0x46B4,
    0xB75B, 0xA77A, 0x9719, 0x8738, 0xF7DF, 0xE7FE, 0xD79D, 0xC7BC,
    0x48C4, 0x58E5, 0x6886, 0x78A7, 0x0840, 0x1861, 0x2802, 0x3823,
    0xC9CC, 0xD9ED, 0xE98E, 0xF9AF, 0x8948, 0x9969, 0xA90A, 0xB92B,
    0x5AF5, 0x4AD4, 0x7AB7, 0x6A96, 0x1A71, 0x0A50, 0x3A33, 0x2A12,
    0xDBFD, 0xCBDC, 0xFBBF, 0xEB9E, 0x9B79, 0x8B58, 0xBB3B, 0xAB1A,
    0x6CA6, 0x7C87, 0x4CE4, 0x5CC5, 0x2C22, 0x3C03, 0x0C60, 0x1C41,
    0xEDAE, 0xFD8F, 0xCDEC, 0xDDCD, 0xAD2A, 0xBD0B, 0x8D68, 0x9D49,
    0x7E97, 0x6EB6, 0x5ED5, 0x4EF4, 0x3E13, 0x2E32, 0x1E51, 0x0E70,
    0xFF9F, 0xEFBE, 0xDFDD, 0xCFFC, 0xBF1B, 0xAF3A, 0x9F59, 0x8F78,
    0x9188, 0x81A9, 0xB1CA, 0xA1EB, 0xD10C, 0xC12D, 0xF14E, 0xE16F,
    0x1080, 0x00A1, 0x30C2, 0x20E3, 0x5004, 0x4025, 0x7046, 0x6067,
    0x83B9, 0x9398, 0xA3FB, 0xB3DA, 0xC33D, 0xD31C, 0xE37F, 0xF35E,
    0x02B1, 0x1290, 0x22F3, 0x32D2, 0x4235, 0x5214, 0x6277, 0x7256,
    0xB5EA, 0xA5CB, 0x95A8, 0x8589, 0xF56E, 0xE54F, 0xD52C, 0xC50D,
    0x34E2, 0x24C3, 0x14A0, 0x0481, 0x7466, 0x6447, 0x5424, 0x4405,
    0xA7DB, 0xB7FA, 0x8799, 0x97B8, 0xE75F, 0xF77E, 0xC71D, 0xD73C,
    0x26D3, 0x36F2, 0x0691, 0x16B0, 0x6657, 0x7676, 0x4615, 0x5634,
    0xD94C, 0xC96D, 0xF90E, 0xE92F, 0x99C8, 0x89E9, 0xB98A, 0xA9AB,
    0x5844, 0x4865, 0x7806, 0x6827, 0x18C0, 0x08E1, 0x3882, 0x28A3,
    0xCB7D, 0xDB5C, 0xEB3F, 0xFB1E, 0x8BF9, 0x9BD8, 0xABBB, 0xBB9A,
    0x4A75, 0x5A54, 0x6A37, 0x7A16, 0x0AF1, 0x1AD0, 0x2AB3, 0x3A92,
    0xFD2E, 0xED0F, 0xDD6C, 0xCD4D, 0xBDAA, 0xAD8B, 0x9DE8, 0x8DC9,
    0x7C26, 0x6C07, 0x5C64, 0x4C45, 0x3CA2, 0x2C83, 0x1CE0, 0x0CC1,
    0xEF1F, 0xFF3E, 0xCF5D, 0xDF7C, 0xAF9B, 0xBFBA, 0x8FD9, 0x9FF8,
    0x6E17, 0x7E36, 0x4E55, 0x5E74, 0x2E93, 0x3EB2, 0x0ED1, 0x1EF0
};
```