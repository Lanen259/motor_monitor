#pragma once
#include <QObject>
#include <functional>
#include <memory>
#include <unordered_map>
#include <vector>
#include <optional>
#include "Topic.h"
#include "../core/message/Message.h"

namespace MotorStudio {

// Data bus — sole data distribution hub (design red-line #2)
class DataBus : public QObject {
    Q_OBJECT
public:
    static DataBus& instance();
    ~DataBus();

    // Single topic publish
    void publish(TopicId topic, float value, uint64_t timestampUs = 0);

    // Frame-based publish (from VofaParser: one frame = multiple channel values)
    // Automatically registers topic IDs if needed
    void publishFrame(const std::vector<TopicId>& topicIds,
                      const QVector<float>& values,
                      uint64_t timestampUs = 0);

    // Batch publish multiple DataPoints at once
    void publishBatch(const std::vector<DataPoint>& points);

    // Subscribe to a single topic
    using SubscriberId = uint64_t;
    SubscriberId subscribe(TopicId topic, std::function<void(const DataPoint&)> callback);
    void unsubscribe(SubscriberId id);

    // Subscribe to multiple topics with one callback
    SubscriberId subscribeMultiple(const std::vector<TopicId>& topics,
                                    std::function<void(const DataPoint&)> callback);

    // Latest value snapshot
    std::optional<float> latestValue(TopicId topic) const;

    // Statistics
    size_t subscriberCount() const;
    size_t publishRate() const;

signals:
    void dataPublished(TopicId topic, float value);
    void framePublished(const QVector<float>& values, uint64_t timestampUs);

private:
    DataBus();
    struct Impl;
    std::unique_ptr<Impl> d;
};

} // namespace MotorStudio
