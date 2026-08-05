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
    static constexpr size_t DEFAULT_CAPACITY = 10000;

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

    // 通道信息
    uint32_t topicId() const { return topicId_; }
    size_t capacity() const { return capacity_; }
    size_t count() const { return count_; }
    size_t totalWritten() const { return totalWritten_; }

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

    // 所有通道 ID
    std::vector<uint32_t> channelIds() const;

    // 高速写入数据点（线程安全）
    void append(uint32_t topicId, uint64_t timestampUs, float value);

    // 批量写入（从 DataBus 订阅）
    void append(const DataPoint& point);

    // 降采样（LTTB 算法）
    std::vector<std::pair<uint64_t, float>> downsample(
        uint32_t topicId, size_t targetPoints) const;

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