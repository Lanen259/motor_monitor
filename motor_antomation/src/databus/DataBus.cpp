#include "DataBus.h"
#include <mutex>
#include <shared_mutex>
#include <chrono>

namespace MotorStudio {

struct SubscriberEntry {
    DataBus::SubscriberId id;
    std::vector<TopicId> topics;
    std::function<void(const DataPoint&)> callback;
};

struct DataBus::Impl {
    std::shared_mutex mutex;
    std::vector<SubscriberEntry> subscribers;
    DataBus::SubscriberId nextId = 1;

    // 最新值缓存
    std::shared_mutex valueMutex;
    std::unordered_map<TopicId, DataPoint> latestValues;

    // 发布速率统计
    std::atomic<size_t> publishCount{0};
    std::chrono::steady_clock::time_point rateStartTime;
    std::atomic<size_t> rateValue{0};

    Impl() {
        rateStartTime = std::chrono::steady_clock::now();
    }
};

DataBus& DataBus::instance() {
    static DataBus bus;
    return bus;
}

DataBus::DataBus() : d(std::make_unique<Impl>()) {}
DataBus::~DataBus() = default;

void DataBus::publish(TopicId topic, float value, uint64_t timestampUs) {
    DataPoint point;
    point.topicId = topic;
    point.value = value;
    point.timestampUs = timestampUs;

    // 更新最新值缓存
    {
        std::unique_lock<std::shared_mutex> lock(d->valueMutex);
        d->latestValues[topic] = point;
    }

    // 更新统计
    d->publishCount++;

    // 通知订阅者
    {
        std::shared_lock<std::shared_mutex> lock(d->mutex);
        for (auto& sub : d->subscribers) {
            // 检查是否订阅了此 topic
            bool matched = false;
            for (auto& t : sub.topics) {
                if (t == topic) {
                    matched = true;
                    break;
                }
            }
            if (matched && sub.callback) {
                sub.callback(point);
            }
        }
    }

    // 发出 Qt 信号
    emit dataPublished(topic, value);
}

DataBus::SubscriberId DataBus::subscribe(TopicId topic, std::function<void(const DataPoint&)> callback) {
    return subscribeMultiple({topic}, std::move(callback));
}

void DataBus::unsubscribe(SubscriberId id) {
    std::unique_lock<std::shared_mutex> lock(d->mutex);
    d->subscribers.erase(
        std::remove_if(d->subscribers.begin(), d->subscribers.end(),
                       [id](const SubscriberEntry& e) { return e.id == id; }),
        d->subscribers.end());
}

DataBus::SubscriberId DataBus::subscribeMultiple(const std::vector<TopicId>& topics,
                                                   std::function<void(const DataPoint&)> callback) {
    std::unique_lock<std::shared_mutex> lock(d->mutex);
    SubscriberId id = d->nextId++;
    d->subscribers.push_back({id, topics, std::move(callback)});
    return id;
}

std::optional<float> DataBus::latestValue(TopicId topic) const {
    std::shared_lock<std::shared_mutex> lock(d->valueMutex);
    auto it = d->latestValues.find(topic);
    if (it != d->latestValues.end()) {
        return it->second.value;
    }
    return std::nullopt;
}

size_t DataBus::subscriberCount() const {
    std::shared_lock<std::shared_mutex> lock(d->mutex);
    return d->subscribers.size();
}

size_t DataBus::publishRate() const {
    auto now = std::chrono::steady_clock::now();
    double elapsed = std::chrono::duration<double>(now - d->rateStartTime).count();
    if (elapsed > 1.0) {
        // 每秒更新一次速率
        d->rateStartTime = now;
        d->rateValue.store(d->publishCount.exchange(0));
        return d->rateValue;
    }
    return d->rateValue;
}

} // namespace MotorStudio