#pragma once
#include <QObject>
#include <memory>
#include <vector>
#include <mutex>
#include <shared_mutex>
#include <atomic>
#include <cstdint>
#include "../databus/Topic.h"
#include "../core/message/Message.h"

namespace MotorStudio {

// ============================================================
// 曲线通道 —— 单通道环形缓冲区（10000点，线程安全）
// ============================================================
class CurveChannel {
public:
    static constexpr size_t DEFAULT_CAPACITY = 100000;

    explicit CurveChannel(uint32_t topicId, size_t capacity = DEFAULT_CAPACITY);
    ~CurveChannel() = default;

    // 高速写入（线程安全）
    void append(uint64_t timestampUs, float value);

    // 读取数据（线程安全）
    // 获取所有数据点
    std::vector<std::pair<uint64_t, float>> allPoints() const;

    // 获取最近 N 个点
    std::vector<std::pair<uint64_t, float>> recentPoints(size_t n) const;

    // 获取数据范围
    struct Range { float minVal; float maxVal; uint64_t minTime; uint64_t maxTime; };
    Range dataRange() const;

    // WF-13: 轻量范围缓存 —— 环形缓冲未回绕（append-only 阶段）时增量维护 min/max，
    // 回绕后按版本号周期性全量重扫，避免每个拉取 tick 都 O(capacity) 扫描。
    Range cachedRange() const;

    // 通道信息
    uint32_t topicId() const { return topicId_; }
    size_t capacity() const { return capacity_; }
    size_t count() const { return count_; }
    size_t totalWritten() const { return totalWritten_; }

    // WF-10: 时间窗口内取点（仅复制 [t0,t1] 内数据，供按窗口降采样，避免全量复制）
    std::vector<std::pair<uint64_t, float>> pointsInRange(
        uint64_t t0Us, uint64_t t1Us) const;

    // 调整环形缓存容量（线程安全）
    void setCapacity(size_t newCapacity);

    // 清空
    void clear();

private:
    uint32_t topicId_;
    size_t capacity_;

    // 环形缓冲区
    mutable std::mutex mutex_;
    std::vector<uint64_t> timestamps_;
    std::vector<float> values_;
    size_t writeIndex_ = 0;
    size_t count_ = 0;
    std::atomic<size_t> totalWritten_{0};

    // WF-13: 范围缓存（append-only 阶段增量维护；回绕后每 kRangeRescan 次写入全量重扫）
    mutable Range cachedRange_{0, 0, 0, 0};
    mutable bool rangeValid_ = false;
    mutable size_t rangeVersion_ = 0;
    static constexpr size_t kRangeRescanEvery = 4096;
    Range scanRangeLocked() const;  // 调用方需持有 mutex_
};

// ============================================================
// 曲线引擎 —— 管理多通道曲线数据
// ============================================================
class CurveEngine : public QObject {
    Q_OBJECT
public:
    explicit CurveEngine(QObject* parent = nullptr);
    ~CurveEngine() override;

    // 通道管理
    void addChannel(uint32_t topicId, size_t capacity = CurveChannel::DEFAULT_CAPACITY);
    void removeChannel(uint32_t topicId);
    void clearChannels();

    // 获取通道
    CurveChannel* channel(uint32_t topicId);
    const CurveChannel* channel(uint32_t topicId) const;

    // 是否有通道
    bool hasChannel(uint32_t topicId) const;

    // 调整指定通道的环形缓存容量
    void setCapacity(uint32_t topicId, size_t capacity);

    // 所有通道 ID
    std::vector<uint32_t> channelIds() const;

    // 高速写入数据点（线程安全）
    void append(uint32_t topicId, uint64_t timestampUs, float value);

    // 批量写入（从 DataBus 订阅）
    void append(const DataPoint& point);

    // 降采样（LTTB 算法）
    std::vector<std::pair<uint64_t, float>> downsample(
        uint32_t topicId, size_t targetPoints) const;

    // WF-10: 时间窗口降采样 —— 只取 [t0Us, t1Us] 窗口内的点再 LTTB，
    // 避免每帧全量复制环形缓冲（100k 点）+ 全量 LTTB 拖垮 GUI 线程。
    std::vector<std::pair<uint64_t, float>> downsampleRange(
        uint32_t topicId, uint64_t t0Us, uint64_t t1Us, size_t targetPoints) const;

    // 数据范围查询
    struct DataRange { float minVal; float maxVal; };
    DataRange dataRange(uint32_t topicId) const;

    // 通道数量
    size_t channelCount() const;

    // 总写入点数
    size_t totalPointsWritten() const;

signals:
    void channelAdded(uint32_t topicId);
    void channelRemoved(uint32_t topicId);
    void dataWritten(uint32_t topicId, uint64_t timestampUs, float value);

private:
    struct Impl;
    std::unique_ptr<Impl> d;
};

} // namespace MotorStudio