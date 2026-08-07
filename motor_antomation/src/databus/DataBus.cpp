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
    mutable std::shared_mutex mutex;
    std::vector<SubscriberEntry> subscribers;
    DataBus::SubscriberId nextId = 1;

    // Latest value cache
    mutable std::shared_mutex valueMutex;
    std::unordered_map<TopicId, DataPoint> latestValues;

    // Publish rate statistics
    std::atomic<size_t> publishCount{0};
    mutable std::chrono::steady_clock::time_point rateStartTime;
    mutable std::atomic<size_t> rateValue{0};

    Impl() {
        rateStartTime = std::chrono::steady_clock::now();
    }

    void notifySubscribers(const DataPoint& point) {
        std::shared_lock<std::shared_mutex> lock(mutex);
        for (auto& sub : subscribers) {
            bool matched = false;
            for (auto& t : sub.topics) {
                if (t == point.topicId) {
                    matched = true;
                    break;
                }
            }
            if (matched && sub.callback) {
                sub.callback(point);
            }
        }
    }
};

DataBus& DataBus::instance() {
    static DataBus bus;
    return bus;
}

DataBus::DataBus() : d(std::make_unique<Impl>()) {}
DataBus::~DataBus() = default;

void DataBus::publish(TopicId topic, float value, uint64_t timestampUs) {
    DataPoint point(topic, value, timestampUs);

    // Update latest value cache
    {
        std::unique_lock<std::shared_mutex> lock(d->valueMutex);
        d->latestValues[topic] = point;
    }

    d->publishCount++;

    // Notify subscribers
    d->notifySubscribers(point);

    // Emit Qt signal
    emit dataPublished(topic, value);
}

void DataBus::publishFrame(const std::vector<TopicId>& topicIds,
                            const QVector<float>& values,
                            uint64_t timestampUs) {
    int count = std::min(static_cast<int>(topicIds.size()), values.size());
    for (int i = 0; i < count; ++i) {
        DataPoint point(topicIds[i], values[i], timestampUs);

        {
            std::unique_lock<std::shared_mutex> lock(d->valueMutex);
            d->latestValues[topicIds[i]] = point;
        }

        d->notifySubscribers(point);
        emit dataPublished(topicIds[i], values[i]);
    }

    d->publishCount += count;
    emit framePublished(values, timestampUs);
}

void DataBus::publishBatch(const std::vector<DataPoint>& points) {
    for (const auto& point : points) {
        {
            std::unique_lock<std::shared_mutex> lock(d->valueMutex);
            d->latestValues[point.topicId] = point;
        }

        d->notifySubscribers(point);
        emit dataPublished(point.topicId, point.value);
    }

    d->publishCount += points.size();
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
        d->rateStartTime = now;
        d->rateValue.store(d->publishCount.exchange(0));
        return d->rateValue;
    }
    return d->rateValue;
}

} // namespace MotorStudio
