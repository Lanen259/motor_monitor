#pragma once
#include <atomic>
#include <vector>
#include <cstddef>
#include <optional>

namespace MotorStudio {

// 无锁 SPSC (Single Producer, Single Consumer) 环形缓冲区
template<typename T, size_t Capacity = 8192>
class RingBuffer {
    static_assert((Capacity & (Capacity - 1)) == 0, "Capacity must be power of 2");

public:
    RingBuffer() : buffer_(Capacity) {}

    bool push(const T& item);
    bool push(T&& item);
    std::optional<T> pop();
    bool empty() const;
    size_t size() const;
    size_t capacity() const { return Capacity; }

private:
    static constexpr size_t mask_ = Capacity - 1;
    std::vector<T> buffer_;
    std::atomic<size_t> writePos_{0};
    std::atomic<size_t> readPos_{0};
};

} // namespace MotorStudio