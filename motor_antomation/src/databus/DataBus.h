#pragma once
#include <QObject>
#include <functional>
#include <memory>
#include <unordered_map>
#include <vector>
#include "Topic.h"
#include "../core/message/Message.h"

namespace MotorStudio {

// 数据总线（核心数据分发中枢）
class DataBus : public QObject {
    Q_OBJECT
public:
    static DataBus& instance();
    ~DataBus();

    // 发布数据
    void publish(TopicId topic, float value, uint64_t timestampUs = 0);

    // 订阅数据
    using SubscriberId = uint64_t;
    SubscriberId subscribe(TopicId topic, std::function<void(const DataPoint&)> callback);
    void unsubscribe(SubscriberId id);

    // 批量订阅
    SubscriberId subscribeMultiple(const std::vector<TopicId>& topics,
                                    std::function<void(const DataPoint&)> callback);

    // 获取最新数据
    std::optional<float> latestValue(TopicId topic) const;

    // 统计
    size_t subscriberCount() const;
    size_t publishRate() const;  // 每秒发布次数

signals:
    void dataPublished(TopicId topic, float value);

private:
    DataBus();
    struct Impl;
    std::unique_ptr<Impl> d;
};

} // namespace MotorStudio