#include "CurveEngine.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace MotorStudio {

// ============================================================
// CurveChannel 实现
// ============================================================

CurveChannel::CurveChannel(uint32_t topicId, size_t capacity)
    : topicId_(topicId)
    , capacity_(capacity)
{
    timestamps_.resize(capacity);
    values_.resize(capacity);
}

void CurveChannel::append(uint64_t timestampUs, float value) {
    std::lock_guard<std::mutex> lock(mutex_);

    timestamps_[writeIndex_] = timestampUs;
    values_[writeIndex_] = value;

    writeIndex_ = (writeIndex_ + 1) % capacity_;
    const bool wrapped = (count_ >= capacity_);
    if (count_ < capacity_) {
        count_++;
    }
    totalWritten_++;

    // WF-13: 增量维护范围缓存，避免每次 dataRange() 都 O(capacity) 全量扫描。
    if (!rangeValid_) {
        cachedRange_ = {value, value, timestampUs, timestampUs};
        rangeValid_ = true;
        rangeVersion_ = totalWritten_.load();
    } else if (!wrapped) {
        // append-only 阶段：增量更新即可
        cachedRange_.minVal = std::min(cachedRange_.minVal, value);
        cachedRange_.maxVal = std::max(cachedRange_.maxVal, value);
        cachedRange_.minTime = std::min(cachedRange_.minTime, timestampUs);
        cachedRange_.maxTime = std::max(cachedRange_.maxTime, timestampUs);
    } else {
        // 回绕后：吸收新点，周期性全量重扫修正（被覆盖的旧极值点）
        cachedRange_.minVal = std::min(cachedRange_.minVal, value);
        cachedRange_.maxVal = std::max(cachedRange_.maxVal, value);
        cachedRange_.minTime = std::min(cachedRange_.minTime, timestampUs);
        cachedRange_.maxTime = std::max(cachedRange_.maxTime, timestampUs);
        if (totalWritten_.load() - rangeVersion_ >= kRangeRescanEvery) {
            cachedRange_ = scanRangeLocked();
            rangeVersion_ = totalWritten_.load();
        }
    }
}

CurveChannel::Range CurveChannel::scanRangeLocked() const {
    Range r;
    r.minVal = std::numeric_limits<float>::max();
    r.maxVal = std::numeric_limits<float>::lowest();
    r.minTime = std::numeric_limits<uint64_t>::max();
    r.maxTime = 0;
    if (count_ == 0) return {0, 0, 0, 0};
    size_t n = (count_ < capacity_) ? count_ : capacity_;
    for (size_t i = 0; i < n; ++i) {
        size_t idx = (count_ < capacity_) ? i : ((writeIndex_ + i) % capacity_);
        r.minVal = std::min(r.minVal, values_[idx]);
        r.maxVal = std::max(r.maxVal, values_[idx]);
        r.minTime = std::min(r.minTime, timestamps_[idx]);
        r.maxTime = std::max(r.maxTime, timestamps_[idx]);
    }
    return r;
}

std::vector<std::pair<uint64_t, float>> CurveChannel::allPoints() const {
    std::lock_guard<std::mutex> lock(mutex_);

    if (count_ == 0) return {};

    std::vector<std::pair<uint64_t, float>> result;
    result.reserve(count_);

    if (count_ < capacity_) {
        // 线性增长阶段
        for (size_t i = 0; i < count_; ++i) {
            result.emplace_back(timestamps_[i], values_[i]);
        }
    } else {
        // 环形缓冲区：从 writeIndex_ 开始（最旧数据）读取
        for (size_t i = 0; i < capacity_; ++i) {
            size_t idx = (writeIndex_ + i) % capacity_;
            result.emplace_back(timestamps_[idx], values_[idx]);
        }
    }

    return result;
}

std::vector<std::pair<uint64_t, float>> CurveChannel::recentPoints(size_t n) const {
    std::lock_guard<std::mutex> lock(mutex_);

    if (count_ == 0 || n == 0) return {};

    size_t actual = std::min(n, count_);
    std::vector<std::pair<uint64_t, float>> result;
    result.reserve(actual);

    for (size_t i = 0; i < actual; ++i) {
        size_t idx = (writeIndex_ + capacity_ - actual + i) % capacity_;
        result.emplace_back(timestamps_[idx], values_[idx]);
    }

    return result;
}

CurveChannel::Range CurveChannel::dataRange() const {
    // WF-13: 命中缓存则直接返回，避免高频拉取 tick 的 O(capacity) 扫描
    std::lock_guard<std::mutex> lock(mutex_);
    if (rangeValid_) return cachedRange_;
    if (count_ == 0) return {0, 0, 0, 0};
    cachedRange_ = scanRangeLocked();
    rangeValid_ = true;
    rangeVersion_ = totalWritten_.load();
    return cachedRange_;
}

CurveChannel::Range CurveChannel::cachedRange() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return rangeValid_ ? cachedRange_ : Range{0, 0, 0, 0};
}

std::vector<std::pair<uint64_t, float>> CurveChannel::pointsInRange(
    uint64_t t0Us, uint64_t t1Us) const {
    std::lock_guard<std::mutex> lock(mutex_);

    if (count_ == 0 || t1Us <= t0Us) return {};

    std::vector<std::pair<uint64_t, float>> result;
    // 时间窗口估计容量（1ms 采样率下的保守上界），避免多次扩容
    size_t est = std::min<size_t>(count_, (t1Us - t0Us) / 1000 + 16);
    result.reserve(est);

    // 环形缓冲按时间有序（从最旧到最新）。扫描到窗口起点，收集窗口内点，越过窗口即停。
    size_t n = (count_ < capacity_) ? count_ : capacity_;
    for (size_t i = 0; i < n; ++i) {
        size_t idx = (count_ < capacity_) ? i : ((writeIndex_ + i) % capacity_);
        uint64_t ts = timestamps_[idx];
        if (ts >= t1Us) break;                 // 已越过窗口（时间有序）
        if (ts >= t0Us) result.emplace_back(ts, values_[idx]);
    }
    return result;
}

void CurveChannel::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    writeIndex_ = 0;
    count_ = 0;
    totalWritten_ = 0;
    rangeValid_ = false;
    rangeVersion_ = 0;
}

void CurveChannel::setCapacity(size_t newCapacity) {
    if (newCapacity == 0) return;

    std::lock_guard<std::mutex> lock(mutex_);

    if (newCapacity == capacity_) return;

    // Allocate new buffers
    std::vector<uint64_t> newTs(newCapacity);
    std::vector<float> newVals(newCapacity);

    // Copy over existing data (up to new capacity, preserving newest points)
    size_t copyCount = std::min(count_, newCapacity);
    if (copyCount > 0) {
        // Read from the oldest point, ignoring gaps
        for (size_t i = 0; i < copyCount; ++i) {
            size_t srcIdx;
            if (count_ < capacity_) {
                // Linear growth: older data at lower indices
                srcIdx = count_ - copyCount + i;
            } else {
                // Wrapped: oldest data starts at writeIndex_
                srcIdx = (writeIndex_ + i) % capacity_;
            }
            newTs[i] = timestamps_[srcIdx];
            newVals[i] = values_[srcIdx];
        }
    }

    timestamps_ = std::move(newTs);
    values_ = std::move(newVals);
    capacity_ = newCapacity;
    writeIndex_ = (copyCount < newCapacity) ? copyCount : 0;
    count_ = copyCount;
    rangeValid_ = false;
    rangeVersion_ = 0;
}

// ============================================================
// CurveEngine 实现
// ============================================================

struct CurveEngine::Impl {
    std::shared_mutex mutex;
    std::vector<std::unique_ptr<CurveChannel>> channels;
};

CurveEngine::CurveEngine(QObject* parent)
    : QObject(parent), d(std::make_unique<Impl>())
{
}

CurveEngine::~CurveEngine() = default;

void CurveEngine::addChannel(uint32_t topicId, size_t capacity) {
    std::unique_lock<std::shared_mutex> lock(d->mutex);

    // 检查是否已存在
    for (auto& ch : d->channels) {
        if (ch->topicId() == topicId) {
            return; // 已存在
        }
    }

    d->channels.push_back(std::make_unique<CurveChannel>(topicId, capacity));
    emit channelAdded(topicId);
}

void CurveEngine::removeChannel(uint32_t topicId) {
    std::unique_lock<std::shared_mutex> lock(d->mutex);

    d->channels.erase(
        std::remove_if(d->channels.begin(), d->channels.end(),
                       [topicId](const auto& ch) { return ch->topicId() == topicId; }),
        d->channels.end());

    emit channelRemoved(topicId);
}

void CurveEngine::clearChannels() {
    std::unique_lock<std::shared_mutex> lock(d->mutex);
    d->channels.clear();
}

void CurveEngine::setCapacity(uint32_t topicId, size_t capacity) {
    auto* ch = channel(topicId);
    if (ch) {
        ch->setCapacity(capacity);
    }
}

CurveChannel* CurveEngine::channel(uint32_t topicId) {
    std::shared_lock<std::shared_mutex> lock(d->mutex);
    for (auto& ch : d->channels) {
        if (ch->topicId() == topicId) {
            return ch.get();
        }
    }
    return nullptr;
}

const CurveChannel* CurveEngine::channel(uint32_t topicId) const {
    std::shared_lock<std::shared_mutex> lock(d->mutex);
    for (auto& ch : d->channels) {
        if (ch->topicId() == topicId) {
            return ch.get();
        }
    }
    return nullptr;
}

bool CurveEngine::hasChannel(uint32_t topicId) const {
    return channel(topicId) != nullptr;
}

std::vector<uint32_t> CurveEngine::channelIds() const {
    std::shared_lock<std::shared_mutex> lock(d->mutex);
    std::vector<uint32_t> ids;
    ids.reserve(d->channels.size());
    for (auto& ch : d->channels) {
        ids.push_back(ch->topicId());
    }
    return ids;
}

void CurveEngine::append(uint32_t topicId, uint64_t timestampUs, float value) {
    auto* ch = channel(topicId);
    if (ch) {
        ch->append(timestampUs, value);
        emit dataWritten(topicId, timestampUs, value);
    }
}

void CurveEngine::append(const DataPoint& point) {
    append(point.topicId, point.timestampUs, point.value);
}

// LTTB (Largest Triangle Three Buckets) 降采样算法（纯函数，供 downsample / downsampleRange 复用）
static std::vector<std::pair<uint64_t, float>> lttbDownsample(
    const std::vector<std::pair<uint64_t, float>>& points, size_t targetPoints)
{
    size_t dataSize = points.size();
    if (dataSize <= targetPoints || targetPoints < 3) {
        return points;
    }

    std::vector<std::pair<uint64_t, float>> result;
    result.reserve(targetPoints);

    // 始终保留首尾点
    result.push_back(points[0]);

    double bucketSize = static_cast<double>(dataSize - 2) / (targetPoints - 2);
    size_t lastSelected = 0;

    for (size_t i = 0; i < targetPoints - 2; ++i) {
        size_t bucketStart = static_cast<size_t>(1 + i * bucketSize);
        size_t bucketEnd = static_cast<size_t>(1 + (i + 1) * bucketSize);
        if (bucketEnd >= dataSize - 1) bucketEnd = dataSize - 2;

        if (bucketStart >= dataSize - 1) break;

        // 计算下一个 bucket 的平均点
        size_t nextBucketStart = bucketEnd;
        size_t nextBucketEnd = static_cast<size_t>(1 + (i + 2) * bucketSize);
        if (nextBucketEnd >= dataSize) nextBucketEnd = dataSize - 1;

        double avgX = 0, avgY = 0;
        size_t nextCount = nextBucketEnd - nextBucketStart;
        if (nextCount == 0) nextCount = 1;

        for (size_t j = nextBucketStart; j < nextBucketEnd; ++j) {
            avgX += static_cast<double>(points[j].first);
            avgY += static_cast<double>(points[j].second);
        }
        avgX /= nextCount;
        avgY /= nextCount;

        // 在 bucket 中找到最大三角形面积的点
        double maxArea = -1.0;
        size_t maxIdx = bucketStart;

        auto& a = points[lastSelected];
        double ax = static_cast<double>(a.first);
        double ay = static_cast<double>(a.second);

        for (size_t j = bucketStart; j < bucketEnd && j < dataSize - 1; ++j) {
            double bx = static_cast<double>(points[j].first);
            double by = static_cast<double>(points[j].second);
            double area = std::abs((ax - avgX) * (by - ay) - (ax - bx) * (avgY - ay)) * 0.5;
            if (area > maxArea) {
                maxArea = area;
                maxIdx = j;
            }
        }

        result.push_back(points[maxIdx]);
        lastSelected = maxIdx;
    }

    result.push_back(points.back());
    return result;
}

std::vector<std::pair<uint64_t, float>> CurveEngine::downsample(
    uint32_t topicId, size_t targetPoints) const
{
    auto* ch = channel(topicId);
    if (!ch) return {};
    auto points = ch->allPoints();
    return lttbDownsample(points, targetPoints);
}

std::vector<std::pair<uint64_t, float>> CurveEngine::downsampleRange(
    uint32_t topicId, uint64_t t0Us, uint64_t t1Us, size_t targetPoints) const
{
    auto* ch = channel(topicId);
    if (!ch) return {};
    auto points = ch->pointsInRange(t0Us, t1Us);
    return lttbDownsample(points, targetPoints);
}

CurveEngine::DataRange CurveEngine::dataRange(uint32_t topicId) const {
    auto* ch = channel(topicId);
    if (!ch) return {0.0f, 0.0f};

    auto range = ch->dataRange();
    return {range.minVal, range.maxVal};
}

size_t CurveEngine::channelCount() const {
    std::shared_lock<std::shared_mutex> lock(d->mutex);
    return d->channels.size();
}

size_t CurveEngine::totalPointsWritten() const {
    std::shared_lock<std::shared_mutex> lock(d->mutex);
    size_t total = 0;
    for (auto& ch : d->channels) {
        total += ch->totalWritten();
    }
    return total;
}

} // namespace MotorStudio