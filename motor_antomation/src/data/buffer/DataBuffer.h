#pragma once
#include <QObject>
#include <memory>
#include <vector>
#include "../../core/message/Message.h"
#include "../../databus/RingBuffer.h"

namespace MotorStudio {

// 数据缓冲区（消费 DataBus 数据，缓存历史数据）
class DataBuffer : public QObject {
    Q_OBJECT
public:
    explicit DataBuffer(size_t maxPoints = 1'000'000, QObject* parent = nullptr);
    ~DataBuffer() override;

    // 追加数据点
    void append(const DataPoint& point);

    // 查询历史数据
    std::vector<DataPoint> queryRange(uint32_t topicId, uint64_t startUs, uint64_t endUs) const;

    // 获取最新 N 个点
    std::vector<DataPoint> latestN(uint32_t topicId, size_t n) const;

    // 清空
    void clear(uint32_t topicId);
    void clearAll();

    size_t pointCount(uint32_t topicId) const;
    size_t totalPoints() const;

private:
    struct Impl;
    std::unique_ptr<Impl> d;
};

} // namespace MotorStudio