# 曲线引擎设计 (Curve Engine Design)

> 版本: 1.0 | 状态: 设计阶段 | 作者: 系统架构组

---

## 1. 目标 (Goal)

构建一套面向电机监控场景的**高性能实时曲线渲染引擎**，实现以下核心目标：

- **实时渲染**：支持 10+ 通道、10kHz 采样率的实时波形绘制，帧率 ≥ 30 FPS。
- **流畅交互**：缩放（X/Y 独立）、平移、暂停/恢复操作无卡顿，延迟 < 50ms。
- **大数据量**：可处理数小时的历史数据，通过三级缓存平滑访问。
- **多窗口同步**：多个曲线窗口共享统一时间轴，联动操作。
- **分析工具**：内置光标测量、标记点、CSV 导出、FFT 预留接口。
- **低资源占用**：GPU 渲染、LTTB 降采样、环形缓冲区，CPU/内存开销可控。

---

## 2. 设计原则 (Design Principles)

| 原则 | 说明 |
|------|------|
| **GPU 渲染优先** | 使用 `QOpenGLWidget` + `QPainter` 硬件加速路径，而非 `QGraphicsView`（CPU 绑定，大量图元时性能不足） |
| **数据与视图分离** | `CurveEngine`（数据层）与 `CurveWidget`（视图层）完全解耦，通过 `DataBus` 订阅数据 |
| **降采样策略** | 当数据点超过屏幕像素宽度时，使用 LTTB 算法降采样，确保视觉保真度 |
| **三级缓存** | GPU → RAM → Disk，热数据在 GPU，温数据在 RAM，冷数据在磁盘 |
| **零拷贝** | 数据在 `RingBuffer` → `CurveEngine` → `CurveWidget` 路径中尽可能使用指针/引用传递 |
| **原子时间轴** | 多窗口通过共享 `TimeRange` 原子对象同步，避免竞态 |
| **渐进增强** | 核心功能（曲线绘制）优先实现，高级功能（FFT、标注）预留接口 |

---

## 3. 为什么不用 QGraphicsView

| 对比维度 | QGraphicsView | QOpenGLWidget + QPainter |
|----------|---------------|--------------------------|
| 渲染后端 | CPU (QPainter raster) | GPU (OpenGL) |
| 10万图元性能 | ~5 FPS (CPU 绑定) | ~60 FPS (GPU 加速) |
| 大数据量 | 必须手动降采样 | 可配合 LTTB 降采样 |
| 抗锯齿 | 软件抗锯齿 | 硬件 MSAA |
| 图层混合 | 逐像素 CPU 合成 | 硬件混合 |
| 内存占用 | 每图元一个对象 | 顶点缓冲，内存可控 |
| 自定义着色器 | 不支持 | 支持 GLSL |

**结论**: 电机监控场景下，单通道 10kHz × 10 通道 = 100k 点/秒，远超 QGraphicsView 的处理能力，必须使用 OpenGL 加速。

---

## 4. 类/模块关系 (Class/Module Relationships)

### 4.1 架构总览

```
┌─────────────────────────────────────────────────────────────┐
│                        CurveWidget                          │
│  (QOpenGLWidget)                                            │
│  ┌──────────────────────────────────────────────────────┐   │
│  │  paintGL()                                            │   │
│  │  ├── 绘制网格 (GridRenderer)                          │   │
│  │  ├── 绘制曲线 (CurveRenderer, GLSL shader)           │   │
│  │  ├── 绘制光标 (CursorRenderer)                       │   │
│  │  ├── 绘制标记 (MarkerRenderer)                       │   │
│  │  └── 绘制图例 (LegendRenderer)                       │   │
│  └──────────────────────────────────────────────────────┘   │
│  ┌──────────────────────────────────────────────────────┐   │
│  │  交互处理:                                              │   │
│  │  ├── 鼠标缩放 (wheelEvent)                             │   │
│  │  ├── 鼠标平移 (mouseMoveEvent)                         │   │
│  │  ├── 光标拖拽 (CursorDragHandler)                      │   │
│  │  └── 触控手势 (GestureHandler)                         │   │
│  └──────────────────────────────────────────────────────┘   │
└──────────────────────────┬──────────────────────────────────┘
                           │ 持有引用
┌──────────────────────────▼──────────────────────────────────┐
│                       CurveEngine                           │
│  ┌──────────────────────────────────────────────────────┐   │
│  │  数据源管理:                                           │   │
│  │  ├── subscribe(channelId)                             │   │
│  │  ├── unsubscribe(channelId)                           │   │
│  │  └── channels: QHash<id, ChannelData>                 │   │
│  ├──────────────────────────────────────────────────────┤   │
│  │  降采样:                                               │   │
│  │  ├── LTTBDownsampler                                  │   │
│  │  └── 降采样缓存 (LTTBCache)                           │   │
│  ├──────────────────────────────────────────────────────┤   │
│  │  三级缓存:                                             │   │
│  │  ├── GpuCache (当前窗口可见数据)                       │   │
│  │  ├── RamCache (最近 10 分钟)                          │   │
│  │  └── DiskCache (全部历史)                             │   │
│  ├──────────────────────────────────────────────────────┤   │
│  │  功能开关:                                             │   │
│  │  ├── paused: bool                                     │   │
│  │  ├── fftEnabled: bool (预留)                          │   │
│  │  └── autoScroll: bool                                 │   │
│  └──────────────────────────────────────────────────────┘   │
└──────────────────────────┬──────────────────────────────────┘
                           │ 订阅
┌──────────────────────────▼──────────────────────────────────┐
│                        DataBus                              │
│  (全局数据总线, 发布-订阅模式)                                │
│  ┌──────────────────────────────────────────────────────┐   │
│  │  publish(channelId, DataPoint)                        │   │
│  │  subscribe(channelId, callback)                       │   │
│  │  unsubscribe(channelId, callback)                     │   │
│  └──────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────┘
```

### 4.2 核心类 UML

```
┌──────────────────────────┐       ┌──────────────────────────┐
│      CurveWidget         │       │       CurveEngine        │
│  (QOpenGLWidget)         │──────▶│                          │
│  - engine: CurveEngine*  │       │  - channels: QHash<>     │
│  - timeRange: TimeRange* │       │  - ramCache: RamCache    │
│  - renderers[]           │       │  - diskCache: DiskCache  │
│  - cursorA, cursorB      │       │  - downsampler: LTTB     │
│  + setTimeRange()        │       │  + subscribe()           │
│  + setYAxisRange()       │       │  + unsubscribe()         │
│  + screenshot()          │       │  + dataForView()         │
│  + exportCSV()           │       │  + setPaused()           │
└──────────────────────────┘       │  + setHistoryRange()     │
                                   └──────────────────────────┘

┌──────────────────────────┐       ┌──────────────────────────┐
│       TimeRange          │       │       LTTBDownsampler    │
│  (atomic, shared)        │       │                          │
│  - start: atomic<int64>  │       │  + downsample(           │
│  - end: atomic<int64>    │       │      data, targetCount)  │
│  + zoom(factor)          │       │  + downsampleWithCache(  │
│  + pan(delta)            │       │      data, targetCount,  │
│  + setRange(s,e)         │       │      cache)              │
└──────────────────────────┘       └──────────────────────────┘

┌──────────────────────────┐       ┌──────────────────────────┐
│      ChannelData         │       │       DataPoint          │
│  - id: QString           │       │  - timestamp: uint64     │
│  - name: QString         │       │  - value: float          │
│  - color: QColor         │       │  - quality: DataQuality  │
│  - yAxisIndex: int       │       └──────────────────────────┘
│  - visible: bool         │
│  - dataBuffer: RingBuf   │
└──────────────────────────┘
```

### 4.3 渲染器模块

```
CurveRenderer (抽象)
├── GridRenderer       — 网格线、刻度标签
├── CurveLineRenderer  — 曲线线段 (GL_LINE_STRIP)
├── CurveFillRenderer  — 曲线填充 (可选)
├── CursorRenderer     — 双光标 (A/B) 及测量值
├── MarkerRenderer     — 标记点 (事件标记)
└── LegendRenderer     — 图例
```

---

## 5. 数据流 (Data Flow)

### 5.1 实时数据流

```
下位机 (10kHz)
    │
    ▼
CommunicationManager → FrameCodec → StreamParser
    │
    ▼
DataBus::publish("motor.current_a", DataPoint{ts, 12.3})
    │
    ▼
CurveEngine::onDataPoint(channelId, dataPoint)
    │
    ├─ 写入 RamCache (RingBuffer)
    ├─ 异步写入 DiskCache (批量写入, 减少 I/O)
    │
    ▼
CurveEngine::updateGpuCache()
    │
    ├─ 计算当前视口时间范围
    ├─ 从 RamCache 获取数据
    ├─ 如果数据点数 > 屏幕像素宽度 × 2:
    │       LTTBDownsampler::downsample(data, pixelWidth)
    └─ 更新 GpuCache (顶点缓冲)
    │
    ▼
CurveWidget::update()  [触发 paintGL]
    │
    ▼
paintGL():
    ├─ CurveRenderer::render(GpuCache, timeRange, yRange)
    ├─ CursorRenderer::render(cursorA, cursorB)
    └─ GridRenderer::render(timeRange, yRange)
```

### 5.2 历史数据回放

```
用户拖动时间轴到过去
    │
    ▼
CurveWidget::setTimeRange(start, end)
    │
    ▼
CurveEngine::setHistoryRange(start, end)
    │
    ├─ 检查 RamCache 是否覆盖 [start, end]
    │   ├─ 是 → 直接从 RamCache 读取
    │   └─ 否 → 从 DiskCache 异步加载
    │         ├─ 加载期间显示 "加载中..." 提示
    │         └─ 加载完成后通知 CurveWidget 更新
    │
    ▼
CurveWidget::update()
```

### 5.3 缩放/平移数据流

```
用户滚轮缩放
    │
    ▼
CurveWidget::wheelEvent(event)
    │
    ├─ 计算缩放中心 (鼠标位置)
    ├─ 计算新的 TimeRange
    │
    ▼
TimeRange::zoom(factor, center)
    │ (原子操作, 线程安全)
    ▼
所有共享此 TimeRange 的 CurveWidget 收到通知
    │
    ▼
CurveWidget::onTimeRangeChanged()
    ├─ 更新轴标签
    ├─ 触发 CurveEngine::updateGpuCache()
    └─ update()
```

---

## 6. API接口规划 (API Interface Planning)

### 6.1 CurveWidget 接口

```cpp
// curvewidget.h
class CurveWidget : public QOpenGLWidget {
    Q_OBJECT
public:
    explicit CurveWidget(QWidget* parent = nullptr);
    ~CurveWidget();

    // 绑定引擎
    void setEngine(CurveEngine* engine);

    // 视图控制
    void setTimeRange(int64_t startUs, int64_t endUs);
    void setYAxisRange(int axisIndex, float min, float max, bool autoScale = false);
    void fitToData();           // 自适应显示
    void zoomToSelection();     // 缩放到选中区域

    // 通道管理
    void addChannel(const QString& channelId);
    void removeChannel(const QString& channelId);
    void setChannelVisible(const QString& channelId, bool visible);
    void setChannelColor(const QString& channelId, const QColor& color);

    // 交互状态
    void setPaused(bool paused);
    bool isPaused() const;
    void setAutoScroll(bool enabled);

    // 光标
    void setCursorA(int64_t timestampUs);   // 设置光标A位置
    void setCursorB(int64_t timestampUs);   // 设置光标B位置
    CursorInfo cursorInfo() const;          // 光标间差值信息

    // 标记
    void addMarker(int64_t timestampUs, const QString& label);
    void removeMarker(int markerId);
    QList<MarkerInfo> markers() const;

    // 导出
    void exportToCSV(const QString& filePath);
    QImage screenshot();

    // 多窗口同步
    void setSharedTimeRange(TimeRange* range);
    TimeRange* sharedTimeRange() const;

signals:
    void cursorMoved(int cursorIndex, int64_t timestampUs);
    void viewRangeChanged(int64_t startUs, int64_t endUs);
    void channelToggled(const QString& channelId, bool visible);

protected:
    void initializeGL() override;
    void resizeGL(int w, int h) override;
    void paintGL() override;
    void wheelEvent(QWheelEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
};
```

### 6.2 CurveEngine 接口

```cpp
// curveengine.h
class CurveEngine : public QObject {
    Q_OBJECT
public:
    explicit CurveEngine(QObject* parent = nullptr);
    ~CurveEngine();

    // 数据订阅
    void subscribe(const QString& channelId);
    void unsubscribe(const QString& channelId);
    QStringList subscribedChannels() const;

    // 数据查询
    QVector<DataPoint> dataForView(const QString& channelId,
                                    int64_t startUs, int64_t endUs) const;
    DataPoint dataAt(const QString& channelId, int64_t timestampUs) const;

    // 统计
    struct ChannelStats {
        double min, max, mean, rms, stdDev;
        int64_t totalPoints;
    };
    ChannelStats statistics(const QString& channelId,
                            int64_t startUs, int64_t endUs) const;

    // 缓存管理
    void setRamCacheDuration(std::chrono::seconds duration);  // 默认 600s
    void setDiskCachePath(const QString& path);
    void clearCache(const QString& channelId = QString());

    // 状态
    void setPaused(bool paused);
    bool isPaused() const;

    // FFT (预留)
    struct FF TResult {
        QVector<float> frequencies;
        QVector<float> magnitudes;
    };
    FF TResult computeFFT(const QString& channelId,
                          int64_t startUs, int64_t endUs,
                          int fftSize = 1024);

    // 多窗口
    TimeRange* sharedTimeRange();

signals:
    void newDataArrived(const QString& channelId);
    void statsUpdated(const QString& channelId);
    void diskCacheProgress(const QString& channelId, int percent);
    void error(const QString& channelId, const QString& message);
};
```

### 6.3 LTTBDownsampler 接口

```cpp
// lttbdownsampler.h
class LTTBDownsampler {
public:
    // 降采样：将 N 个点降至 targetCount 个点
    // 算法复杂度: O(N)
    static QVector<DataPoint> downsample(
        const DataPoint* data, size_t count, size_t targetCount);

    // 带缓存的降采样（用于相同数据重复降采样到不同分辨率）
    static QVector<DataPoint> downsampleWithCache(
        const DataPoint* data, size_t count, size_t targetCount,
        LTTBCache& cache);

    // 预计算多级缓存（常用于缩放操作）
    static void buildPyramidCache(
        const DataPoint* data, size_t count,
        LTTBCache& cache,
        const QVector<size_t>& targetCounts);
};

// LTTB 缓存结构
class LTTBCache {
public:
    void put(size_t targetCount, QVector<DataPoint> result);
    std::optional<QVector<DataPoint>> get(size_t targetCount) const;
    void invalidate();
    size_t memoryUsage() const;

private:
    QHash<size_t, QVector<DataPoint>> m_cache;
    mutable QReadWriteLock m_lock;
};
```

### 6.4 TimeRange 接口

```cpp
// timerange.h
class TimeRange {
public:
    TimeRange(int64_t startUs = 0, int64_t endUs = 0);

    // 原子读取
    int64_t start() const;   // atomic load
    int64_t end() const;     // atomic load
    int64_t duration() const;

    // 原子写入
    void setRange(int64_t start, int64_t end);
    void zoom(double factor, int64_t centerUs = -1);
    void pan(int64_t deltaUs);
    void panToNow(int64_t windowDurationUs);

    // 范围检查
    bool contains(int64_t timestampUs) const;
    double positionToFraction(int64_t timestampUs) const;

    // 信号（变化通知）
    void setCallback(std::function<void(int64_t, int64_t)> callback);

private:
    std::atomic<int64_t> m_start;
    std::atomic<int64_t> m_end;
    std::function<void(int64_t, int64_t)> m_callback;
};
```

---

## 7. LTTB 降采样算法

### 7.1 算法原理

LTTB (Largest Triangle Three Buckets) 是时间序列可视化的最优降采样算法：

- 将数据点等分为 `targetCount` 个桶（buckets）
- 每个桶选择 1 个代表点，使得连续三个桶的代表点形成的三角形面积最大
- 始终保留首尾点

```
输入: data[0..N-1], targetCount = T
输出: sampled[0..T-1]

1. sampled[0] = data[0]  (保留首点)
2. 将 data[1..N-1] 等分为 T-1 个桶
3. 对每个桶 i (1..T-2):
   a. 桶 i 中每个候选点 p:
      计算三角形面积: area(prev_selected, p, 桶i+1的平均点)
   b. 选择面积最大的点
4. sampled[T-1] = data[N-1]  (保留尾点)
```

### 7.2 实现要点

```cpp
QVector<DataPoint> LTTBDownsampler::downsample(
    const DataPoint* data, size_t count, size_t targetCount)
{
    if (count <= targetCount) {
        return QVector<DataPoint>(data, data + count);
    }

    QVector<DataPoint> result(targetCount);
    result[0] = data[0];  // 保留首点

    const size_t bucketSize = (count - 2) / (targetCount - 2);
    size_t prevIdx = 0;

    for (size_t i = 1; i < targetCount - 1; ++i) {
        const size_t bucketStart = 1 + (i - 1) * bucketSize;
        const size_t bucketEnd = std::min(bucketStart + bucketSize, count - 1);
        const size_t avgX = (bucketStart + bucketEnd) / 2;

        // 计算桶内平均点 (用于三角形面积)
        double avgY = 0;
        for (size_t j = bucketStart; j < bucketEnd; ++j) {
            avgY += data[j].value;
        }
        avgY /= (bucketEnd - bucketStart);

        double maxArea = -1;
        size_t maxIdx = bucketStart;

        for (size_t j = bucketStart; j < bucketEnd; ++j) {
            // 三角形面积 = |(x1(y2-y3) + x2(y3-y1) + x3(y1-y2)) / 2|
            double area = std::abs(
                (data[prevIdx].timestamp - data[j].timestamp) * (avgY - data[prevIdx].value) -
                (data[prevIdx].timestamp - avgX) * (data[j].value - data[prevIdx].value)
            ) * 0.5;
            if (area > maxArea) {
                maxArea = area;
                maxIdx = j;
            }
        }

        result[i] = data[maxIdx];
        prevIdx = maxIdx;
    }

    result[targetCount - 1] = data[count - 1];  // 保留尾点
    return result;
}
```

---

## 8. 三级缓存设计

### 8.1 缓存层级

```
┌─────────────────────────────────────────────────────────┐
│  GPU Cache (VRAM)                                       │
│  - 容量: 当前视口可见数据 (顶点缓冲)                      │
│  - 延迟: < 1ms (GPU 直接访问)                           │
│  - 更新: 每次视口变化时从 RamCache 更新                  │
├─────────────────────────────────────────────────────────┤
│  RAM Cache (系统内存)                                    │
│  - 容量: 最近 10 分钟数据 (RingBuffer)                   │
│  - 延迟: < 10μs (内存访问)                              │
│  - 更新: 实时追加，环形覆盖                              │
├─────────────────────────────────────────────────────────┤
│  Disk Cache (SSD/HDD)                                   │
│  - 容量: 全部历史数据                                   │
│  - 延迟: 1-10ms (SSD) / 10-50ms (HDD)                  │
│  - 格式: 每通道独立文件, 二进制格式 (时间戳索引)         │
│  - 更新: 批量写入 (每秒一次), 减少 I/O 次数              │
└─────────────────────────────────────────────────────────┘
```

### 8.2 磁盘缓存格式

```
文件: cache/motor.current_a.bin

Header (64B):
  [magic:4B][version:4B][sample_rate:4B][data_type:4B]
  [total_points:8B][start_time:8B][end_time:8B][reserved:24B]

Index Block (每 10000 点一个索引):
  [point_offset:8B][timestamp:8B] × N

Data Block:
  [timestamp:8B][value:4B] × N
```

---

## 9. 多窗口同步机制

### 9.1 共享 TimeRange

```
┌──────────────┐     ┌──────────────┐     ┌──────────────┐
│ CurveWidget 1│     │ CurveWidget 2│     │ CurveWidget 3│
│  (电流波形)   │     │  (速度波形)   │     │  (FFT 频谱)  │
└──────┬───────┘     └──────┬───────┘     └──────┬───────┘
       │                    │                    │
       └────────────────────┼────────────────────┘
                            │
                   ┌────────▼────────┐
                   │    TimeRange    │
                   │  start: atomic  │
                   │  end:   atomic  │
                   │  callback()     │
                   └─────────────────┘
```

### 9.2 同步流程

```cpp
// 创建共享 TimeRange
auto sharedRange = new TimeRange(0, 10'000'000); // 10秒窗口

// 多个窗口绑定同一个 TimeRange
widget1->setSharedTimeRange(sharedRange);
widget2->setSharedTimeRange(sharedRange);
widget3->setSharedTimeRange(sharedRange);

// 任一窗口缩放/平移，其他窗口自动同步
// TimeRange 内部使用 std::atomic 保证线程安全
// 变化时通过 callback 通知所有 CurveWidget
```

---

## 10. 功能清单

| 功能 | 状态 | 描述 |
|------|------|------|
| 多通道叠加 | ✅ 核心 | 支持 10+ 通道在同一视图叠加显示 |
| 独立 Y 轴 | ✅ 核心 | 每个通道可绑定到左侧或右侧 Y 轴，独立缩放 |
| 暂停/恢复 | ✅ 核心 | 暂停后冻结视图，新数据仍写入缓存 |
| X 轴缩放 | ✅ 核心 | 滚轮缩放，支持对数/线性 |
| Y 轴缩放 | ✅ 核心 | Ctrl+滚轮 或 拖拽 Y 轴 |
| 平移 | ✅ 核心 | 鼠标拖拽，暂停时可用 |
| 历史缓存 | ✅ 核心 | 暂停后向左拖动查看历史数据 |
| 标记点 | ✅ 增强 | 用户可在曲线上添加文字标记 |
| 双光标 | ✅ 增强 | 光标 A/B 测量时间差和值差 |
| FFT | 🔲 预留 | 频谱分析，接口已定义 |
| CSV 导出 | ✅ 增强 | 导出当前视口数据为 CSV |
| 截图 | ✅ 增强 | 导出当前视图为 PNG |
| 多窗口同步 | ✅ 增强 | 共享 TimeRange 实现 |
| 自动滚动 | ✅ 核心 | 实时模式自动滚动到最新数据 |
| 触控手势 | ✅ 增强 | 双指缩放、平移 |

---

## 11. 后续实现注意事项 (Implementation Notes)

| 类别 | 注意事项 |
|------|----------|
| **OpenGL 上下文** | 确保 `paintGL()` 在正确的线程中调用；`QOpenGLWidget` 的渲染在 GUI 线程，数据更新需通过信号槽跨线程 |
| **顶点缓冲更新** | 使用 `glBufferSubData` 而非 `glBufferData` 进行增量更新，避免重新分配 GPU 内存 |
| **LTTB 性能** | 10 万点降至 2000 点，LTTB 耗时约 2-3ms，需在后台线程执行，结果通过信号返回 |
| **降采样缓存** | 相同数据范围重复降采样时（如频繁缩放），利用 LTTBCache 避免重复计算 |
| **内存预算** | 10 通道 × 10kHz × 10 分钟 = 6000 万点 ≈ 720MB (每点 12B: 8B ts + 4B val)，RamCache 需限制在 512MB 以内 |
| **磁盘缓存** | 使用 mmap 或 QFile::map 提高读取速度；索引块加速时间范围查询；定期清理旧数据 |
| **Y 轴自动缩放** | 自动缩放需考虑所有可见通道的值域，排除异常值（±3σ 外），避免曲线被极端值压扁 |
| **抗锯齿** | 启用 MSAA 4x (`QSurfaceFormat::setSamples(4)`)，曲线使用 `GL_LINE_SMOOTH` 或自定义宽线着色器 |
| **网格对齐** | 时间轴刻度自动对齐到 1ms/10ms/100ms/1s/10s 等整数刻度，避免漂移 |
| **无闪烁** | 使用双缓冲（Qt 默认），所有绘制在 `paintGL()` 中完成，禁止在 `paintGL()` 外调用 OpenGL |
| **单元测试** | LTTB 降采样准确性（与原始数据视觉对比）、TimeRange 原子操作正确性、缓存读写一致性 |
| **性能基准** | 10 通道 × 10kHz 实时模式下，CPU 占用 < 15%，GPU 占用 < 30%，帧率 ≥ 30 FPS |

---

## 附录 A: 顶点着色器

```glsl
// curve.vert
#version 330 core
layout(location = 0) in vec2 a_pos;        // 顶点位置 (time, value)
layout(location = 1) in vec4 a_color;      // 通道颜色

uniform mat4 u_mvp;                         // 模型-视图-投影矩阵

out vec4 v_color;

void main() {
    gl_Position = u_mvp * vec4(a_pos, 0.0, 1.0);
    v_color = a_color;
}
```

## 附录 B: 片元着色器

```glsl
// curve.frag
#version 330 core
in vec4 v_color;
out vec4 fragColor;

void main() {
    fragColor = v_color;
}
```

## 附录 C: 交互快捷键

| 快捷键 | 操作 |
|--------|------|
| 滚轮 | X 轴缩放 |
| Ctrl + 滚轮 | Y 轴缩放 |
| 鼠标拖拽 | 平移 |
| 空格键 | 暂停/恢复 |
| R 键 | 自适应显示 |
| C 键 | 添加标记 |
| A/D 键 | 移动光标 A |
| ←/→ 键 | 移动光标 B |
| Ctrl+S | 导出 CSV |
| Ctrl+P | 截图 |