#pragma once
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>
#include <mutex>

namespace MotorStudio {

using TopicId = uint32_t;

// Channel descriptor — defines what a channel IS
struct ChannelDescriptor {
    TopicId topicId = 0;
    std::string name;       // e.g. "Ia", "Speed", "Torque"
    std::string unit;       // e.g. "A", "RPM", "N·m"
    std::string dataType;   // "float", "int32", "uint16"
    float scale = 1.0f;
    float offset = 0.0f;
    float defaultValue = 0.0f;
    uint32_t color = 0xFF888888;   // ARGB, default gray
};

// Runtime topic registry — replaces hardcoded Topics namespace
class TopicRegistry {
public:
    static TopicRegistry& instance();

    // Register a new topic or return existing ID
    TopicId registerTopic(const std::string& name);
    TopicId registerTopic(const ChannelDescriptor& desc);

    // Lookup
    std::string topicName(TopicId id) const;
    TopicId findTopic(const std::string& name) const;
    ChannelDescriptor descriptor(TopicId id) const;

    // Auto-register from a list of names (used when VofaParser parses first frame)
    std::vector<TopicId> registerTopics(const std::vector<std::string>& names);

    // All registered topics
    std::vector<TopicId> allTopicIds() const;
    size_t count() const;

    // Remove a topic by ID
    bool removeTopic(TopicId id);

private:
    TopicRegistry() = default;
    mutable std::mutex mutex_;
    std::unordered_map<std::string, TopicId> nameToId_;
    std::unordered_map<TopicId, ChannelDescriptor> idToDesc_;
    TopicId nextId_ = 1;
};

// Legacy: keep predefined IDs for backward compat (will be phased out in P1-01)
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
