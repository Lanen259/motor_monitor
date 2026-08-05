# 数据缓冲设计 (Data Buffer Design)

> 版本: 1.0 | 状态: 设计阶段 | 作者: 系统架构组

---

## 1. 目标 (Goal)

设计一套面向高频实时数据采集的**无锁环形缓冲区 (Lock-Free Ring Buffer)**，实现以下核心目标：

- **极致性能**：单次 push/pop 操作 < 1μs，支持 10kHz × 10 通道 = 100k 点/秒的持续写入。
- **无锁并发**：SPSC (Single Producer, Single Consumer) 模型，无互斥锁，无内存分配。
- **零拷贝**：批量读取接口支持 `std::span` 视图，避免数据拷贝。
- **丢包检测**：通过序列号机制检测数据丢失，提供统计信息。
- **缓存友好**：Cache-line 对齐，避免 false sharing，最大化 CPU 缓存命中率。
- **确定性**：无动态内存分配，无异常抛出，适合实时系统。

---

## 2. 设计原则 (Design Principles)

| 原则 | 说明 |
|------|------|
| **Lock-Free** | 使用 `std::atomic` + memory ordering 实现无锁 SPSC，`acquire`/`release` 语义保证可见性 |
| **Power of 2** | 容量必须是 2 的幂，使用位运算 `index & (capacity - 1)` 替代取模，节省 CPU 周期 |
| **Cache-Line 对齐** | 读写索引位于不同 cache line（通常 64 字节），避免 false sharing |
| **零拷贝批量读取** | `bulkRead()` 返回指向内部缓冲区的 `std::span`，消费者直接读取，无需拷贝 |
| **序列号单调** | 每个元素携带全局递增序列号，消费者可检测丢包 |
| **覆写最旧** | 环形缓冲区满时自动覆写最旧数据，保证最新数据始终可用 |
| **RAII** | 缓冲区在构造时分配，析构时释放，生命周期内无额外分配 |

---

## 3. 类/模块关系 (Class/Module Relationships)

### 3.1 架构总览

```
┌─────────────────────────────────────────────────────────┐
│                    DataBus                               │
│  publish(channelId, DataPoint)                          │
│  subscribe(channelId, callback)                         │
└──────────────────────┬──────────────────────────────────┘
                       │ 每个 channel
                       ▼
┌─────────────────────────────────────────────────────────┐
│              RingBuffer<DataPoint, 1048576>              │
│  (1M 容量, 约 12MB)                                     │
│  ┌───────────────────────────────────────────────────┐  │
│  │  Producer Side (StreamParser 线程)                 │  │
│  │  - push(const T&) / pushBatch(span)               │  │
│  │  - writeIndex: atomic<size_t>                     │  │
│  │  - sequence: atomic<uint64_t>                     │  │
│  ├───────────────────────────────────────────────────┤  │
│  │  Consumer Side (CurveEngine 线程)                  │  │
│  │  - pop() / bulkRead(maxCount)                     │  │
│  │  - readIndex: atomic<size_t>                      │  │
│  │  - lastSequence: uint64_t                         │  │
│  ├───────────────────────────────────────────────────┤  │
│  │  Data: T[Capacity] (cache-line aligned)           │  │
│  └───────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────┘
```

### 3.2 核心类 UML

```
┌──────────────────────────────────────────────────────┐
│           RingBuffer<T, Capacity>                     │
│  (Template, Capacity 必须是 2 的幂)                    │
├──────────────────────────────────────────────────────┤
│  - m_buffer: alignas(64) T[Capacity]                  │
│  - m_writeIdx: alignas(64) std::atomic<size_t>        │
│  - m_readIdx:  alignas(64) std::atomic<size_t>        │
│  - m_sequence:          std::atomic<uint64_t>         │
│  - m_lastReadSeq:       uint64_t                      │
├──────────────────────────────────────────────────────┤
│  + push(const T&) -> bool                             │
│  + pushBatch(span<const T>) -> size_t                 │
│  + pop() -> std::optional<T>                          │
│  + bulkRead(maxCount) -> span<const T>                │
│  + available() -> size_t                              │
│  + capacity() -> size_t                               │
│  + isFull() -> bool                                   │
│  + isEmpty() -> bool                                  │
│  + reset()                                            │
│  + stats() -> BufferStats                             │
└──────────────────────────────────────────────────────┘

┌──────────────────────────────────────────────────────┐
│                  BufferStats                          │
├──────────────────────────────────────────────────────┤
│  + totalPushed: uint64_t                              │
│  + totalPopped: uint64_t                              │
│  + totalOverwrites: uint64_t                          │
│  + lostSequences: uint64_t                            │
│  + currentSize: size_t                                │
│  + peakSize: size_t                                   │
│  + avgPushTimeNs: double                              │
│  + avgPopTimeNs: double                               │
└──────────────────────────────────────────────────────┘
```

### 3.3 与上层模块的关系

```
StreamParser (Producer)
    │
    │ push/pushBatch
    ▼
RingBuffer<DataPoint, Capacity>
    │
    │ bulkRead / pop
    ▼
CurveEngine (Consumer) → LTTBDownsampler → CurveWidget
```

---

## 4. 数据流 (Data Flow)

### 4.1 Push 流程

```
Producer Thread:
    │
    ▼
push(const T& item)
    │
    ├─ 获取当前 writeIdx (atomic load, memory_order_relaxed)
    ├─ 写入 m_buffer[writeIdx] = item
    ├─ 更新 m_sequence++ (atomic fetch_add, memory_order_release)
    ├─ 更新 writeIdx = (writeIdx + 1) & (Capacity - 1)  (atomic store, memory_order_release)
    │
    └─ 返回 true (或 false 若缓冲区满且覆写策略不允许)
```

### 4.2 Pop 流程

```
Consumer Thread:
    │
    ▼
pop()
    │
    ├─ 获取当前 readIdx (atomic load, memory_order_acquire)
    ├─ 获取当前 writeIdx (atomic load, memory_order_acquire)
    ├─ 如果 readIdx == writeIdx: 返回 nullopt (缓冲区空)
    ├─ 读取 m_buffer[readIdx]
    ├─ 检查序列号连续性 (m_lastReadSeq vs item.sequence)
    │   ├─ 连续: 更新 m_lastReadSeq
    │   └─ 不连续: 记录 lostSequences++
    ├─ 更新 readIdx = (readIdx + 1) & (Capacity - 1) (atomic store, memory_order_release)
    │
    └─ 返回 item
```

### 4.3 批量读取流程

```
Consumer Thread:
    │
    ▼
bulkRead(maxCount)
    │
    ├─ 获取 readIdx, writeIdx (atomic load, acquire)
    ├─ 计算 available = 可读元素数
    ├─ 计算连续可读长度 (考虑环形边界)
    │   contiguous = min(available, Capacity - readIdx)
    │   count = min(contiguous, maxCount)
    ├─ 返回 span<const T>(&m_buffer[readIdx], count)
    │   └─ 零拷贝！消费者直接访问内部缓冲区
    ├─ 更新 readIdx += count (atomic store, release)
    │
    └─ 调用者使用完毕后，span 自动失效 (下次 push 可能覆写)
```

### 4.4 序列号丢包检测

```
Timeline:
  Producer:  [seq=100] [seq=101] [seq=102] [seq=103] [seq=104]
  Consumer:  [seq=100] ──skip── [seq=102] ──skip── [seq=104]
                              ↑ lost=1            ↑ lost=1

检测逻辑:
  if (item.sequence != m_lastReadSeq + 1) {
      m_stats.lostSequences += item.sequence - m_lastReadSeq - 1;
      emit dataLossDetected(lostCount);
  }
  m_lastReadSeq = item.sequence;
```

---

## 5. API接口规划 (API Interface Planning)

### 5.1 RingBuffer 模板类

```cpp
// ringbuffer.h
#pragma once

#include <atomic>
#include <optional>
#include <span>
#include <cstddef>
#include <cstdint>

template<typename T, size_t Capacity>
class RingBuffer {
    static_assert((Capacity & (Capacity - 1)) == 0, 
                  "Capacity must be a power of 2");
    static_assert(Capacity > 0, "Capacity must be > 0");

public:
    RingBuffer();
    ~RingBuffer() = default;

    // 禁止拷贝
    RingBuffer(const RingBuffer&) = delete;
    RingBuffer& operator=(const RingBuffer&) = delete;

    // ── Producer API ──

    // 推送单个元素 (线程安全, 仅 Producer 调用)
    // 返回 true 表示成功, false 表示覆写了旧数据
    bool push(const T& item);

    // 批量推送 (线程安全, 仅 Producer 调用)
    // 返回实际推送的元素数
    size_t pushBatch(std::span<const T> items);

    // ── Consumer API ──

    // 弹出单个元素 (线程安全, 仅 Consumer 调用)
    // 返回 nullopt 表示缓冲区空
    std::optional<T> pop();

    // 批量读取 (线程安全, 仅 Consumer 调用)
    // 返回零拷贝视图，调用者需在下次 pop/bulkRead 前消费完毕
    std::span<const T> bulkRead(size_t maxCount = Capacity);

    // 确认消费 (配合 bulkRead 使用)
    void commitRead(size_t count);

    // ── 状态查询 (线程安全, 任意线程) ──

    // 可读元素数
    size_t available() const;

    // 剩余可写空间
    size_t remaining() const;

    // 是否满
    bool isFull() const;

    // 是否空
    bool isEmpty() const;

    // 容量
    static constexpr size_t capacity() { return Capacity; }

    // ── 管理 ──

    // 重置缓冲区 (非线程安全, 仅在所有线程停止时调用)
    void reset();

    // 获取统计信息
    struct BufferStats {
        uint64_t totalPushed;
        uint64_t totalPopped;
        uint64_t totalOverwrites;
        uint64_t lostSequences;
        size_t   currentSize;
        size_t   peakSize;
    };
    BufferStats stats() const;

private:
    static constexpr size_t CACHE_LINE_SIZE = 64;
    static constexpr size_t MASK = Capacity - 1;

    // 缓冲区: cache-line 对齐, 避免 false sharing
    alignas(CACHE_LINE_SIZE) T m_buffer[Capacity];

    // 写索引: 独立 cache line, 仅 Producer 写入
    alignas(CACHE_LINE_SIZE) std::atomic<size_t> m_writeIdx{0};

    // 读索引: 独立 cache line, 仅 Consumer 写入
    alignas(CACHE_LINE_SIZE) std::atomic<size_t> m_readIdx{0};

    // 全局序列号 (Producer 写入, Consumer 读取)
    alignas(CACHE_LINE_SIZE) std::atomic<uint64_t> m_sequence{0};

    // 统计 (非原子, 通过 memory ordering 保证可见性)
    alignas(CACHE_LINE_SIZE) uint64_t m_totalPushed{0};
    uint64_t m_totalPopped{0};
    uint64_t m_totalOverwrites{0};
    uint64_t m_lostSequences{0};
    size_t m_peakSize{0};
    uint64_t m_lastReadSeq{0};  // Consumer 的最后读取序列号

    // 性能统计
    uint64_t m_pushTimeAccumNs{0};
    uint64_t m_popTimeAccumNs{0};
};
```

### 5.2 实现核心代码

```cpp
// ── push: Producer 线程 ──
template<typename T, size_t Capacity>
bool RingBuffer<T, Capacity>::push(const T& item) {
    const size_t writeIdx = m_writeIdx.load(std::memory_order_relaxed);
    const size_t readIdx  = m_readIdx.load(std::memory_order_acquire);

    bool overwrote = false;
    if ((writeIdx + 1) & MASK == (readIdx & MASK)) {
        // 缓冲区满，覆写最旧数据
        overwrote = true;
        m_totalOverwrites++;
        // 推进读指针 (Consumer 跳过最旧数据)
        m_readIdx.store((readIdx + 1) & MASK, std::memory_order_release);
    }

    // 写入数据
    m_buffer[writeIdx] = item;

    // 更新序列号 (release 保证数据写入先于索引更新可见)
    const uint64_t seq = m_sequence.fetch_add(1, std::memory_order_release);

    // 更新写索引 (release 保证 Consumer 能读到完整数据)
    m_writeIdx.store((writeIdx + 1) & MASK, std::memory_order_release);

    // 更新统计
    m_totalPushed++;
    const size_t currentSize = available();
    if (currentSize > m_peakSize) {
        m_peakSize = currentSize;
    }

    return !overwrote;
}

// ── pop: Consumer 线程 ──
template<typename T, size_t Capacity>
std::optional<T> RingBuffer<T, Capacity>::pop() {
    const size_t readIdx  = m_readIdx.load(std::memory_order_relaxed);
    const size_t writeIdx = m_writeIdx.load(std::memory_order_acquire);

    if (readIdx == writeIdx) {
        return std::nullopt;  // 缓冲区空
    }

    // 读取数据 (acquire 保证读到 push 写入的完整数据)
    T item = m_buffer[readIdx];

    // 更新读索引 (release 保证后续 push 能看到读指针推进)
    m_readIdx.store((readIdx + 1) & MASK, std::memory_order_release);

    m_totalPopped++;
    return item;
}

// ── bulkRead: 零拷贝批量读取 ──
template<typename T, size_t Capacity>
std::span<const T> RingBuffer<T, Capacity>::bulkRead(size_t maxCount) {
    const size_t readIdx  = m_readIdx.load(std::memory_order_relaxed);
    const size_t writeIdx = m_writeIdx.load(std::memory_order_acquire);

    size_t available = (writeIdx - readIdx) & MASK;  // 使用位运算
    if (available == 0) {
        return std::span<const T>();
    }

    // 计算连续可读长度 (考虑环形边界)
    size_t contiguous = std::min(available, Capacity - readIdx);
    size_t count = std::min(contiguous, maxCount);

    // 返回零拷贝视图
    return std::span<const T>(&m_buffer[readIdx], count);
}

template<typename T, size_t Capacity>
void RingBuffer<T, Capacity>::commitRead(size_t count) {
    const size_t readIdx = m_readIdx.load(std::memory_order_relaxed);
    m_readIdx.store((readIdx + count) & MASK, std::memory_order_release);
    m_totalPopped += count;
}

// ── available: 可读元素数 ──
template<typename T, size_t Capacity>
size_t RingBuffer<T, Capacity>::available() const {
    const size_t writeIdx = m_writeIdx.load(std::memory_order_acquire);
    const size_t readIdx  = m_readIdx.load(std::memory_order_acquire);
    return (writeIdx - readIdx) & MASK;
}
```

---

## 6. 内存布局 (Memory Layout)

### 6.1 Cache-Line 对齐

```
Cache Line 0 (64B):  m_buffer[0..63/sizeof(T)]
Cache Line 1 (64B):  m_buffer[64/sizeof(T)..]
...
Cache Line N (64B):  m_writeIdx (独立 cache line, 避免 false sharing)
Cache Line N+1 (64B):  m_readIdx  (独立 cache line, 避免 false sharing)
Cache Line N+2 (64B):  m_sequence (独立 cache line)
Cache Line N+3 (64B):  统计字段
```

### 6.2 False Sharing 问题

```
❌ 错误: 两个索引在同一 cache line
┌───────────────────────────────┐
│ writeIdx | readIdx | ...      │  Cache Line (64B)
└───────────────────────────────┘
  Producer 写入 writeIdx → 使整个 cache line 失效
  Consumer 读取 readIdx → 缓存未命中！
  结果: 每次操作都触发 cache miss, 性能下降 50-100×

✅ 正确: 每个索引独立 cache line
┌───────────────────────────────┐
│ writeIdx (填充到 64B)          │  Cache Line N
└───────────────────────────────┘
┌───────────────────────────────┐
│ readIdx  (填充到 64B)          │  Cache Line N+1
└───────────────────────────────┘
  Producer 写入 writeIdx → 仅 Cache Line N 失效
  Consumer 读取 readIdx → Cache Line N+1 命中！
```

### 6.3 位运算取模

```cpp
// 容量为 2 的幂时:
index & (Capacity - 1)  等价于  index % Capacity

// 性能对比 (x86_64):
//   DIV 指令:  ~30 cycles
//   AND 指令:  ~1 cycle
//   加速: 30×

// 差值计算可用位运算:
// (writeIdx - readIdx) & MASK  → 等效于模运算差值
```

---

## 7. 性能分析

### 7.1 时间复杂度

| 操作 | 时间复杂度 | 说明 |
|------|-----------|------|
| `push()` | O(1) | 两次 atomic load, 一次 store, 无锁 |
| `pop()` | O(1) | 两次 atomic load, 一次 store, 无锁 |
| `bulkRead()` | O(1) | 返回 span, 无拷贝 |
| `available()` | O(1) | 两次 atomic load |
| `pushBatch()` | O(N) | 批量写入, 单次 atomic store |

### 7.2 延迟分析

```
单次 push 操作 (x86_64, 3GHz):
  atomic load (writeIdx):   ~1 ns
  atomic load (readIdx):    ~1 ns
  写入数据:                  ~1 ns
  atomic store (writeIdx):  ~1 ns
  ─────────────────────────────
  总计:                     ~4 ns

单次 pop 操作:
  atomic load (readIdx):    ~1 ns
  atomic load (writeIdx):   ~1 ns
  读取数据:                  ~1 ns
  atomic store (readIdx):   ~1 ns
  ─────────────────────────────
  总计:                     ~4 ns

安全边际: < 1μs (含统计更新、T 的构造/析构)
```

### 7.3 吞吐量分析

```
10 通道 × 10kHz = 100,000 点/秒

每点 push 耗时: ~4 ns
每点 pop 耗时:  ~4 ns

100,000 × 4ns = 400μs/秒 (CPU 占用 0.04%)

结论: RingBuffer 远非瓶颈, 实际瓶颈在 CurveEngine 降采样和渲染。
```

### 7.4 容量选择

| 通道数 | 采样率 | 缓存时长 | 所需容量 | 内存 (12B/pt) |
|--------|--------|----------|----------|---------------|
| 1 | 10kHz | 10 分钟 | 6,000,000 | 72 MB |
| 10 | 10kHz | 10 分钟 | 60,000,000 | 720 MB |
| 10 | 1kHz | 10 分钟 | 600,000 | 7.2 MB |
| 10 | 10kHz | 1 分钟 | 6,000,000 | 72 MB |

**建议**: 每个通道独立 RingBuffer，容量 2^20 = 1,048,576 (约 1M)，内存约 12MB/通道。

---

## 8. 数据保留策略 (Data Retention Policy)

### 8.1 环形覆写

```
策略: 环形缓冲区满时，自动覆写最旧数据

行为:
  push() 时发现 (writeIdx + 1) % Capacity == readIdx:
    1. 推进 readIdx (跳过最旧数据)
    2. totalOverwrites++
    3. 写入新数据到 writeIdx 位置
    4. 推进 writeIdx

影响:
  - Consumer 如果处理速度不够快，可能丢失旧数据
  - 但最新数据始终可用（实时监控场景优先）
  - Consumer 通过 lostSequences 感知丢包
```

### 8.2 背压处理

```
当 Consumer 持续落后:
  1. RingBuffer 满 → 覆写旧数据
  2. totalOverwrites 持续增长
  3. 当 totalOverwrites/分钟 > 阈值时:
     a. 通知用户 "数据积压，可能存在性能问题"
     b. 自动降低采样率 (如 10kHz → 1kHz)
     c. CurveEngine 自动增加降采样因子
```

---

## 9. 与 DataPoint 的集成

### 9.1 DataPoint 结构

```cpp
// datapoint.h
struct alignas(16) DataPoint {
    uint64_t timestamp;   // 微秒时间戳 (8B)
    float    value;       // 采样值 (4B)
    uint32_t sequence;    // 全局序列号 (4B), 用于丢包检测
    // 总计: 16 字节, 对齐到 16B (SSE 友好)

    DataPoint() : timestamp(0), value(0.0f), sequence(0) {}
    DataPoint(uint64_t ts, float val, uint32_t seq)
        : timestamp(ts), value(val), sequence(seq) {}
};

static_assert(sizeof(DataPoint) == 16, "DataPoint must be 16 bytes");
static_assert(alignof(DataPoint) == 16, "DataPoint must be 16-byte aligned");
```

### 9.2 使用示例

```cpp
// 创建缓冲区 (每通道 1M 点)
RingBuffer<DataPoint, 1'048'576> currentBuffer;

// Producer: StreamParser 线程
void StreamParser::onDataPoint(const DataPoint& dp) {
    bool ok = currentBuffer.push(dp);
    if (!ok) {
        // 缓冲区满，覆写了旧数据
        qCWarning(log.comm.stream) << "Buffer overflow, data overwritten";
    }
}

// Consumer: CurveEngine 线程
void CurveEngine::processData() {
    // 批量读取 (零拷贝)
    auto span = currentBuffer.bulkRead(10000);
    
    if (!span.empty()) {
        // 直接使用 span，无需拷贝
        auto downsampled = m_downsampler.downsample(span.data(), span.size(), 2000);
        updateGpuCache(downsampled);
        
        // 确认消费
        currentBuffer.commitRead(span.size());
    }
}
```

---

## 10. 后续实现注意事项 (Implementation Notes)

| 类别 | 注意事项 |
|------|----------|
| **容量约束** | 模板参数 `Capacity` 必须编译期检查为 2 的幂(`static_assert`)，否则位运算取模错误 |
| **Memory Ordering** | 严格使用 `acquire`/`release` 语义；`writeIdx` 的 store 使用 `release`，`readIdx` 的 load 使用 `acquire` |
| **SPSC 约束** | 仅支持单生产者单消费者；多生产者/消费者需要改用 `compare_exchange` 或更复杂的 MPMC 算法 |
| **T 的要求** | `T` 必须满足 `TriviallyCopyable`（用于零拷贝批量读取），且析构函数不应有副作用（环形覆写时不会调用析构） |
| **对齐** | `DataPoint` 使用 `alignas(16)` 对齐，配合 SSE/AVX 向量化；`m_buffer` 使用 `alignas(64)` 对齐到 cache line |
| **性能测试** | 使用 `std::chrono::high_resolution_clock` 测量 `push`/`pop` 延迟，p99 应 < 100ns |
| **边界测试** | 测试缓冲区满时的覆写行为、序列号回绕 (uint64_t 回绕约需 5.8 亿年)、并发的正确性 |
| **线程安全** | `reset()` 不是线程安全的，必须在所有读写线程停止后调用；`stats()` 读取非原子字段存在轻微不一致，可接受 |
| **内存映射** | 对于超大缓冲区 (如 1GB)，考虑使用 `mmap` 匿名映射，而非堆分配，避免虚拟内存碎片 |
| **跨平台** | `std::atomic` 在 MSVC / GCC / Clang 上行为一致；`std::span` 需要 C++20，或使用 `gsl::span` 作为替代 |
| **调试支持** | 在 Debug 模式下，可选启用 `_ITERATOR_DEBUG_LEVEL=0` 加速 STL；提供 `dump()` 调试函数输出缓冲区状态 |

---

## 附录 A: 基准测试代码

```cpp
#include <chrono>
#include <iostream>

void benchmarkPush() {
    RingBuffer<DataPoint, 1'048'576> buffer;
    constexpr int ITERATIONS = 10'000'000;

    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < ITERATIONS; ++i) {
        buffer.push(DataPoint{i, float(i) * 0.1f, uint32_t(i)});
    }
    auto end = std::chrono::high_resolution_clock::now();

    auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
    std::cout << "Push: " << (double)ns / ITERATIONS << " ns/op" << std::endl;
    // 预期: < 5 ns/op
}

void benchmarkPop() {
    RingBuffer<DataPoint, 1'048'576> buffer;
    constexpr int ITERATIONS = 10'000'000;

    // 预填充
    for (int i = 0; i < ITERATIONS; ++i) {
        buffer.push(DataPoint{i, float(i) * 0.1f, uint32_t(i)});
    }

    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < ITERATIONS; ++i) {
        auto item = buffer.pop();
        (void)item;
    }
    auto end = std::chrono::high_resolution_clock::now();

    auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
    std::cout << "Pop: " << (double)ns / ITERATIONS << " ns/op" << std::endl;
    // 预期: < 5 ns/op
}
```

## 附录 B: 常见陷阱

| 陷阱 | 说明 | 解决方案 |
|------|------|----------|
| False Sharing | 读写索引在同一 cache line | 使用 `alignas(64)` 隔离 |
| ABA 问题 | 索引回绕后无法区分新旧数据 | 配合全局序列号 `m_sequence` |
| 消费者悬挂 | Consumer 崩溃后 Producer 持续写入 | 添加心跳检测，超时后重置缓冲区 |
| 溢出未检测 | 容量非 2 的幂导致 `& MASK` 错误 | `static_assert` 编译期检查 |
| 跨线程析构 | 缓冲区析构时仍有线程在写 | 使用引用计数或确保线程先停止 |
| 类型不对齐 | `T` 未对齐导致 SIMD 访问失败 | `static_assert(alignof(T) >= alignof(std::max_align_t))` 或使用 `alignas` |