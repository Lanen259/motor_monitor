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
    if (count_ < capacity_) {
        count_++;
    }
    totalWritten_++;
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
    std::lock_guard<std::mutex> lock(mutex_);

    Range r;
    r.minVal = std::numeric_limits<float>::max();
    r.maxVal = std::numeric_limits<float>::lowest();
    r.minTime = std::numeric_limits<uint64_t>::max();
    r.maxTime = 0;

    if (count_ == 0) {
        r.minVal = 0;
        r.maxVal = 0;
        return r;
    }

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

void CurveChannel::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    writeIndex_ = 0;
    count_ = 0;
    totalWritten_ = 0;
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

// LTTB (Largest Triangle Three Buckets) 降采样算法
std::vector<std::pair<uint64_t, float>> CurveEngine::downsample(
    uint32_t topicId, size_t targetPoints) const
{
    auto* ch = channel(topicId);
    if (!ch) return {};

    auto points = ch->allPoints();
    if (points.size() <= targetPoints || targetPoints < 3) {
        return points;
    }

    size_t dataSize = points.size();
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