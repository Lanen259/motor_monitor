#pragma once
#include <cstdint>
#include <string>
#include <unordered_map>

namespace MotorStudio {

// Topic ID（整数，避免字符串匹配开销）
using TopicId = uint32_t;

// Topic 注册表
class TopicRegistry {
public:
    static TopicRegistry& instance();

    TopicId registerTopic(const std::string& name);
    std::string topicName(TopicId id) const;
    TopicId findTopic(const std::string& name) const;

private:
    TopicRegistry() = default;
    std::unordered_map<std::string, TopicId> nameToId_;
    std::unordered_map<TopicId, std::string> idToName_;
    TopicId nextId_ = 1;
};

// 预定义 Topic ID（常用变量）
namespace Topics {
    constexpr TopicId Ia       = 1;
    constexpr TopicId Ib       = 2;
    constexpr TopicId Ic       = 3;
    constexpr TopicId Id       = 4;
    constexpr TopicId Iq       = 5;
    constexpr TopicId Speed    = 6;
    constexpr TopicId Position = 7;
    constexpr TopicId Voltage  = 8;
    constexpr TopicId Current  = 9;
    constexpr TopicId Temperature = 10;
    constexpr TopicId Fault    = 11;
    constexpr TopicId Timestamp = 12;
}

} // namespace MotorStudio