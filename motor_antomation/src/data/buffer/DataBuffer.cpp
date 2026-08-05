#include "DataBuffer.h"

namespace MotorStudio {

struct DataBuffer::Impl {
    size_t maxPoints = 1'000'000;
};

DataBuffer::DataBuffer(size_t maxPoints, QObject* parent)
    : QObject(parent), d(std::make_unique<Impl>()) {
    d->maxPoints = maxPoints;
}

DataBuffer::~DataBuffer() = default;

void DataBuffer::append(const DataPoint& point) {
    // TODO: 追加到环形缓冲区
}

std::vector<DataPoint> DataBuffer::queryRange(uint32_t topicId, uint64_t startUs, uint64_t endUs) const {
    return {};
}

std::vector<DataPoint> DataBuffer::latestN(uint32_t topicId, size_t n) const {
    return {};
}

void DataBuffer::clear(uint32_t topicId) {}
void DataBuffer::clearAll() {}

size_t DataBuffer::pointCount(uint32_t topicId) const { return 0; }
size_t DataBuffer::totalPoints() const { return 0; }

} // namespace MotorStudio