# ThreadModel — 线程模型

> **文档版本**: v1.0  
> **父文档**: [SystemArchitecture.md](SystemArchitecture.md)  

---

## 目标

定义 Motor Studio 的线程架构，明确线程职责、通信方式、安全策略和生命周期管理。确保系统在高负载（1kHz/100+ 变量）下不出现死锁、优先级反转或数据竞争。

---

## 设计原则

| 原则 | 说明 |
|------|------|
| **最少线程** | 仅 5-6 个持久线程 + 1 个共享线程池，避免线程爆炸 |
| **职责隔离** | 每个线程只处理一类任务，禁止跨职责混用 |
| **无锁优先** | 热路径线程间通信使用 Lock-free Queue / RingBuffer |
| **信号槽安全** | 跨线程 Qt 信号使用 `Qt::QueuedConnection` |
| **显式生命周期** | 每个线程有明确的启动/停止/等待超时机制 |
| **死锁预防** | 所有锁获取顺序一致，禁止嵌套锁，使用 `std::lock` 统一获取多锁 |

---

## 线程拓扑

```
┌─────────────────────────────────────────────────────────────────────────┐
│                          Thread Architecture                              │
│                                                                          │
│  ┌──────────────┐                                                        │
│  │  Main Thread  │  Qt Event Loop, UI Rendering, User Input              │
│  │  (UI Thread)  │  ViewModel logic, ConfigManager, PluginLoader         │
│  └──────┬───────┘                                                        │
│         │ Qt::QueuedConnection                                          │
│         ▼                                                                │
│  ┌──────────────┐     Lock-free Queue                                    │
│  │  DataBus     │◄──────────────────────────────┐                        │
│  │  Thread      │  Pub/Sub, RingBuffer, Cache   │                        │
│  └──────┬───────┘                                │                        │
│         │ Lock-free Queue                        │                        │
│         ▼                                        │                        │
│  ┌──────────────┐     ┌──────────────┐          │                        │
│  │  Comm Thread │     │  Comm Thread │  ...     │  (1 per connection)    │
│  │  (Serial #1) │     │  (CAN #1)    │          │                        │
│  └──────────────┘     └──────────────┘          │                        │
│         │                                        │                        │
│         └────────────────────────────────────────┘                        │
│                                                                          │
│  ┌──────────────┐                                                        │
│  │ Logger Thread│  spdlog async sink, Disk I/O                          │
│  └──────────────┘                                                        │
│                                                                          │
│  ┌──────────────────────────────────────────────────────────────────┐   │
│  │                        WorkerPool (shared)                         │   │
│  │  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐             │   │
│  │  │ Curve Rendering│  │ Automation   │  │ Script Exec  │  ...       │   │
│  │  └──────────────┘  └──────────────┘  └──────────────┘             │   │
│  └──────────────────────────────────────────────────────────────────┘   │
│                                                                          │
│  ┌──────────────┐                                                        │
│  │  Watchdog    │  Monitors DataBus heartbeat, triggers recovery         │
│  │  Thread      │                                                        │
│  └──────────────┘                                                        │
└─────────────────────────────────────────────────────────────────────────┘
```

---

## 线程详细定义

---

### 1. UI Thread (Main Thread)

| 属性 | 值 |
|------|-----|
| **线程 ID** | `main` |
| **优先级** | Normal |
| **事件循环** | Qt Event Loop (`QApplication::exec()`) |
| **生命周期** | `main()` 开始 → `QApplication::quit()` 结束 |

**职责**：
- Qt 事件循环，QML 渲染
- 用户输入处理（鼠标/键盘/触摸）
- ViewModel 逻辑执行（非 CPU 密集型）
- 系统初始化与关闭编排
- ConfigManager、PluginLoader、CrashHandler 运行于此线程

**限制**：
- 禁止阻塞操作（同步 I/O、长时间计算）
- 禁止直接操作 DataBus 内部数据结构（必须通过线程安全接口）
- 禁止直接调用 Transport 的 `read/write`（必须通过 DataBus 中转）

**线程间通信**：
- 与 DataBus Thread：通过 `IDataBus` 接口的内部 Lock-free Queue
- 与 WorkerPool：通过 `QThreadPool::start()` 提交任务
- 与 Logger：通过 `spdlog` 异步队列（线程安全）

---

### 2. DataBus Thread

| 属性 | 值 |
|------|-----|
| **线程 ID** | `databus` |
| **优先级** | High（或实时调度） |
| **事件循环** | 自定义事件循环（`while(running) { process(); }`） |
| **生命周期** | `DataBus::init()` → `DataBus::shutdown()` |

**职责**：
- 发布/订阅引擎：Topic 匹配与消息分发
- 上行数据接收：从 Comm Thread 接收原始 DataPoint，写入 RingBuffer 并广播给订阅者
- 下行命令转发：从 CommandQueue 取命令，转发到 Protocol 层
- 数据缓存更新：最新值存储，过期检测
- Watchdog 心跳发送

**内部数据结构**（全部线程安全）：
```
┌─────────────────────────────────────────────────────────┐
│                    DataBus Thread                         │
│                                                          │
│  ┌──────────────────┐   ┌──────────────────┐            │
│  │  TopicRegistry   │   │  SubscriptionMap │            │
│  │  (TopicId→Meta)  │   │  (TopicId→[Sub]) │            │
│  └──────────────────┘   └──────────────────┘            │
│                                                          │
│  ┌──────────────────────────────────────────────────┐   │
│  │               RingBuffer Pool                     │   │
│  │  ┌──────────┐  ┌──────────┐  ┌──────────┐        │   │
│  │  │ Topic 1  │  │ Topic 2  │  │ Topic N  │  ...   │   │
│  │  │ (1MB)    │  │ (1MB)    │  │ (1MB)    │        │   │
│  │  └──────────┘  └──────────┘  └──────────┘        │   │
│  └──────────────────────────────────────────────────┘   │
│                                                          │
│  ┌──────────────────┐   ┌──────────────────┐            │
│  │   DataCache      │   │  CommandQueue    │            │
│  │ (TopicId→latest) │   │  (priority heap) │            │
│  └──────────────────┘   └──────────────────┘            │
│                                                          │
│  ┌──────────────────┐                                   │
│  │   Watchdog       │  Heartbeat counter                │
│  │   Heartbeat      │                                   │
│  └──────────────────┘                                   │
└─────────────────────────────────────────────────────────┘
```

**处理循环**：
```cpp
void DataBus::run() {
    while (running_) {
        // 1. 处理上行数据（Comm Thread → DataBus）
        processIncomingData();

        // 2. 处理订阅分发（广播给所有订阅者）
        dispatchSubscriptions();

        // 3. 处理下行命令（CommandQueue → Protocol）
        processOutgoingCommands();

        // 4. 更新 Watchdog 心跳
        heartbeat_counter_.fetch_add(1, std::memory_order_release);

        // 5. 等待下一个周期（或事件驱动唤醒）
        waitForNextCycle();
    }
}
```

---

### 3. Communication Thread(s)

| 属性 | 值 |
|------|-----|
| **线程 ID** | `comm_<port_id>` |
| **实例数** | 每个物理连接 1 个线程 |
| **优先级** | High |
| **事件循环** | Transport 内部 I/O 循环 |
| **生命周期** | `Transport::open()` → `Transport::close()` |

**职责**：
- 物理层 I/O 操作（串口/CAN/TCP 读写）
- 原始字节流收发
- 连接状态监控（断线检测）
- 自动重连逻辑

**线程分配策略**：
```
Connection 1 (Serial COM3)  →  CommThread_1
Connection 2 (CAN can0)     →  CommThread_2
Connection 3 (TCP 192.168.1.100:502) → CommThread_3
```

**与 DataBus Thread 通信**：
```
CommThread                              DataBus Thread
    │                                         │
    │  ┌─────────────────────────┐            │
    ├──► Lock-free SPSC Queue    ├───────────►│
    │  │ (RawDataPoint)           │            │
    │  └─────────────────────────┘            │
    │                                         │
```

---

### 4. Logger Thread

| 属性 | 值 |
|------|-----|
| **线程 ID** | `logger` |
| **优先级** | Low |
| **事件循环** | spdlog 异步线程池 |
| **生命周期** | `Logger::init()` → `Logger::shutdown()` |

**职责**：
- 异步日志写入磁盘
- 日志轮转（按日/按大小）
- 日志级别过滤
- 多 sink 分发（文件 + 控制台 + 网络）

**与其他线程通信**：
```
Any Thread ──[spdlog::logger::log()]──► Lock-free Queue ──► Logger Thread ──► Disk
```

---

### 5. WorkerPool

| 属性 | 值 |
|------|-----|
| **线程 ID** | `worker_<N>` |
| **实例数** | `QThreadPool`（默认 `std::thread::hardware_concurrency()` 个线程） |
| **优先级** | Normal |
| **生命周期** | 全局单例，与应用同生命周期 |

**职责**：
- CPU 密集型任务执行
- 曲线渲染数据预处理（降采样、插值）
- 自动化脚本执行（Python/Lua 沙箱）
- 数据导出（CSV/Excel/PDF）
- 周期性数据聚合计算

**任务提交**：
```cpp
// 曲线渲染任务
QThreadPool::globalInstance()->start([this]() {
    auto render_data = prepareCurveData(request);
    emit renderDataReady(render_data);
});

// Qt Concurrent 简化写法
auto future = QtConcurrent::run(&CurveEngineService::prepareCurveData, this, request);
```

---

### 6. Watchdog Thread

| 属性 | 值 |
|------|-----|
| **线程 ID** | `watchdog` |
| **优先级** | High（实时） |
| **事件循环** | 简单定时轮询 |
| **生命周期** | `DataBus::init()` 之后启动，`DataBus::shutdown()` 之前停止 |

**职责**：
- 监控 DataBus Thread 心跳计数器
- 超时检测（默认 3s 无心跳变化）
- 触发恢复回调（由 Application 层决策）

**检测逻辑**：
```cpp
void Watchdog::run() {
    while (running_) {
        auto current = databus_->getHeartbeatCount();
        std::this_thread::sleep_for(std::chrono::milliseconds(500));

        auto next = databus_->getHeartbeatCount();
        if (current == next) {
            stale_count_++;
            if (stale_count_ >= max_stale_count_) {  // 6 * 500ms = 3s
                on_timeout_callback_();
                stale_count_ = 0;
            }
        } else {
            stale_count_ = 0;
        }
    }
}
```

---

## 线程间通信方法

### 通信机制选型

| 通信路径 | 机制 | 理由 |
|----------|------|------|
| Comm → DataBus | Lock-free SPSC Queue | 单生产者/单消费者，高性能，无锁 |
| DataBus → UI | Qt QueuedConnection | 利用 Qt 事件循环，无需额外同步 |
| DataBus → WorkerPool | Qt QueuedConnection / QThreadPool::start | 异步任务提交 |
| Any → Logger | spdlog async queue | spdlog 内置无锁队列 |
| DataBus → Watchdog | `std::atomic<uint64_t>` | 原子变量，无锁读取 |
| ConfigManager 读写 | `std::shared_mutex` | 读多写少，读写锁优化 |

### Lock-free SPSC Queue

```cpp
// 用于 Comm Thread → DataBus Thread 的高性能数据传递
template<typename T, size_t Capacity>
class SPSCQueue {
    static_assert((Capacity & (Capacity - 1)) == 0, "Capacity must be power of 2");

    std::array<T, Capacity> buffer_;
    std::atomic<size_t> write_pos_{0};
    std::atomic<size_t> read_pos_{0};

public:
    bool try_push(const T& item) {
        auto w = write_pos_.load(std::memory_order_relaxed);
        auto r = read_pos_.load(std::memory_order_acquire);
        if (w - r >= Capacity) return false;  // full
        buffer_[w & (Capacity - 1)] = item;
        write_pos_.store(w + 1, std::memory_order_release);
        return true;
    }

    bool try_pop(T& item) {
        auto r = read_pos_.load(std::memory_order_relaxed);
        auto w = write_pos_.load(std::memory_order_acquire);
        if (r >= w) return false;  // empty
        item = buffer_[r & (Capacity - 1)];
        read_pos_.store(r + 1, std::memory_order_release);
        return true;
    }
};
```

### RingBuffer（零拷贝共享内存）

```cpp
// 用于 DataBus 内部的高性能历史数据缓存
// 允许多个订阅者并发读取（无锁读），单生产者写入
template<typename T, size_t Capacity>
class RingBuffer {
    static_assert((Capacity & (Capacity - 1)) == 0, "Capacity must be power of 2");

    std::array<T, Capacity> buffer_;
    std::atomic<uint64_t> write_seq_{0};  // 全局写入序号

public:
    // 写入（单生产者）
    uint64_t push(const T& item) {
        auto seq = write_seq_.fetch_add(1, std::memory_order_acq_rel);
        buffer_[seq & (Capacity - 1)] = item;
        return seq;
    }

    // 读取最新（无锁，多消费者安全）
    const T* get_latest() const {
        auto seq = write_seq_.load(std::memory_order_acquire);
        if (seq == 0) return nullptr;
        return &buffer_[(seq - 1) & (Capacity - 1)];
    }

    // 批量读取（无锁）
    std::vector<T> get_range(uint64_t from_seq, uint64_t to_seq) const {
        std::vector<T> result;
        for (auto seq = from_seq; seq < to_seq; ++seq) {
            result.push_back(buffer_[seq & (Capacity - 1)]);
        }
        return result;
    }
};
```

---

## 线程安全策略

### 数据访问规则

| 数据 | 所有者线程 | 读取者 | 写入者 | 同步机制 |
|------|------------|--------|--------|----------|
| TopicRegistry | DataBus | DataBus | DataBus | 单线程，无需同步 |
| SubscriptionMap | DataBus | DataBus | DataBus | 单线程，无需同步 |
| RingBuffer | DataBus | DataBus, WorkerPool | DataBus | 原子序号 + 单生产者 |
| DataCache | DataBus | DataBus, UI (读) | DataBus | `std::shared_mutex` |
| CommandQueue | DataBus | DataBus | UI, WorkerPool | Lock-free SPSC Queue |
| ConfigStore | Main | 任意线程（读） | Main（写） | `std::shared_mutex` |
| TransportState | Comm | Comm | Comm | 单线程，无需同步 |

### 线程安全级别

```cpp
// 线程安全级别注释约定
// @thread_safety: single       — 仅单线程访问
// @thread_safety: mutex        — std::mutex 保护
// @thread_safety: rw_mutex     — std::shared_mutex 保护
// @thread_safety: atomic       — std::atomic 保护
// @thread_safety: lock_free    — Lock-free 数据结构
// @thread_safety: queued        — 通过 Qt QueuedConnection 同步
```

---

## 优雅关闭序列

### 关闭顺序

```
Step 1: UI Layer
    ├── 停止接受用户输入
    ├── 保存窗口状态
    └── 关闭所有 ViewModel 订阅

Step 2: Service Layer
    ├── AutomationEngineService::shutdown() — 停止所有脚本
    ├── CurveEngineService::shutdown()     — 停止渲染
    └── DataMonitorService::shutdown()     — 停止监控

Step 3: DataBus
    ├── 停止接受新订阅
    ├── 清空 CommandQueue
    ├── 通知所有订阅者关闭
    └── 等待所有 Comm Thread 退出

Step 4: Protocol Layer
    ├── 每个 Protocol::shutdown()
    └── ProtocolManager::shutdown()

Step 5: Transport Layer
    ├── 每个 Transport::close()
    ├── 等待 Comm Thread 退出（超时 5s）
    └── TransportManager::shutdown()

Step 6: Watchdog
    └── Watchdog::stop()

Step 7: Infrastructure
    ├── PluginLoader::unloadAll()
    ├── CrashHandler::uninstall()
    ├── Logger::shutdown() — 最后关闭，确保所有日志写入
    └── ConfigManager::save()
```

### 关闭超时

| 阶段 | 超时时间 | 超时行为 |
|------|----------|----------|
| Service 停止 | 3s | 强制终止，日志记录 |
| DataBus 清空 | 2s | 丢弃未处理命令，日志记录 |
| Protocol 关闭 | 2s | 强制 detach |
| Transport 关闭 | 5s | 强制 close，日志记录 |
| Comm Thread 退出 | 5s | `QThread::terminate()`（最后手段） |
| Logger 刷新 | 3s | 强制 flush + 退出 |

---

## 死锁预防规则

### 锁获取规则

1. **全局锁顺序**：`ConfigManager → DataBus → Transport → Logger`（禁止反向获取）
2. **禁止嵌套锁**：持有锁 A 时禁止获取锁 B（除非 A 在 B 之前的顺序中）
3. **使用 `std::lock`**：必须同时获取多锁时，使用 `std::lock(l1, l2)` 原子获取
4. **禁止在持有锁时调用回调**：回调可能触发反向锁获取
5. **使用 `std::unique_lock` + `std::defer_lock`**：延迟获取，便于 `std::lock` 统一管理

### 规则检查

```cpp
// ❌ 错误：持有锁时调用回调
void DataBus::publish(TopicId topic, DataPoint data) {
    std::lock_guard lock(mutex_);
    for (auto& sub : subscribers_[topic]) {
        sub.callback(data);  // 危险！回调可能尝试获取其他锁
    }
}

// ✅ 正确：先复制订阅者列表，再释放锁后调用
void DataBus::publish(TopicId topic, DataPoint data) {
    std::vector<SubscriberCallback> callbacks;
    {
        std::lock_guard lock(mutex_);
        for (auto& sub : subscribers_[topic]) {
            callbacks.push_back(sub.callback);
        }
    }
    for (auto& cb : callbacks) {
        cb(data);  // 安全：锁已释放
    }
}
```

---

## 后续实现注意事项

1. **线程优先级验证**：在目标平台上验证 `QThread::setPriority()` 实际效果，Linux 可能需要 `SCHED_FIFO` 策略（需 root 或 `CAP_SYS_NICE`）
2. **SPSC Queue 容量调优**：初始容量 4096，根据实际通信吞吐量动态调整；提供 `overflow_count` 统计
3. **Qt 对象线程亲和性**：使用 `QObject::moveToThread()` 迁移对象到目标线程，确保 `signal/slot` 自动选择正确的连接类型
4. **WorkerPool 任务超时**：每个 Worker 任务需设置超时（默认 30s），超时后强制终止并记录日志
5. **Comm Thread 数量上限**：默认最多 8 个 Comm Thread，超过时通过 TransportManager 排队等待
6. **Watchdog 恢复策略**：Watchdog 只检测+通知，不决策；由 Application 层根据上下文决定：重启 DataBus / 全局重启 / 仅日志告警
7. **线程命名**：所有线程使用 `QThread::setObjectName()` 或 `pthread_setname_np()` 命名，便于调试器和日志识别
8. **性能计数器**：每个线程导出 `processed_count` / `error_count` / `avg_latency_us` 指标，供性能监控面板使用
9. **关闭死锁检测**：在 Debug 模式下，关闭阶段增加 10s 全局超时，超时后打印所有线程调用栈并 `std::abort()`
10. **CI 线程安全测试**：添加 ThreadSanitizer (TSan) 构建配置，CI 定期运行线程安全检测