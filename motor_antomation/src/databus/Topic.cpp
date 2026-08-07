#include "Topic.h"
#include <algorithm>

namespace MotorStudio {

TopicRegistry& TopicRegistry::instance()
{
    static TopicRegistry registry;
    return registry;
}

TopicId TopicRegistry::registerTopic(const std::string& name)
{
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = nameToId_.find(name);
    if (it != nameToId_.end()) {
        return it->second;
    }
    TopicId id = nextId_++;
    nameToId_.emplace(name, id);
    idToDesc_[id] = ChannelDescriptor{id, name, "", "float", 1.0f, 0.0f, 0.0f, 0xFF888888};
    return id;
}

TopicId TopicRegistry::registerTopic(const ChannelDescriptor& desc)
{
    std::lock_guard<std::mutex> lock(mutex_);

    // Priority 1: If descriptor already has a topicId, update by ID (supports rename)
    if (desc.topicId != 0) {
        auto idIt = idToDesc_.find(desc.topicId);
        if (idIt != idToDesc_.end()) {
            // Name changed: remove old name mapping, add new one
            const std::string& oldName = idIt->second.name;
            if (oldName != desc.name) {
                nameToId_.erase(oldName);
            }
            nameToId_[desc.name] = desc.topicId;
            idToDesc_[desc.topicId] = desc;
            return desc.topicId;
        }
        // topicId not found in registry — fall through to name-based lookup
    }

    // Priority 2: Name-based lookup
    auto it = nameToId_.find(desc.name);
    if (it != nameToId_.end()) {
        // Update existing descriptor in-place (allows re-registration with new properties)
        ChannelDescriptor d = desc;
        d.topicId = it->second;
        idToDesc_[it->second] = d;
        return it->second;
    }

    // Priority 3: Brand-new topic
    TopicId id = nextId_++;
    nameToId_.emplace(desc.name, id);
    ChannelDescriptor d = desc;
    d.topicId = id;
    idToDesc_[id] = d;
    return id;
}

std::string TopicRegistry::topicName(TopicId id) const
{
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = idToDesc_.find(id);
    return (it != idToDesc_.end()) ? it->second.name : std::string();
}

TopicId TopicRegistry::findTopic(const std::string& name) const
{
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = nameToId_.find(name);
    return (it != nameToId_.end()) ? it->second : 0;
}

ChannelDescriptor TopicRegistry::descriptor(TopicId id) const
{
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = idToDesc_.find(id);
    if (it != idToDesc_.end()) {
        return it->second;
    }
    return ChannelDescriptor{};
}

std::vector<TopicId> TopicRegistry::registerTopics(const std::vector<std::string>& names)
{
    std::vector<TopicId> ids;
    ids.reserve(names.size());
    for (const auto& name : names) {
        ids.push_back(registerTopic(name));
    }
    return ids;
}

std::vector<TopicId> TopicRegistry::allTopicIds() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<TopicId> ids;
    ids.reserve(idToDesc_.size());
    for (const auto& pair : idToDesc_) {
        ids.push_back(pair.first);
    }
    return ids;
}

size_t TopicRegistry::count() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return idToDesc_.size();
}

bool TopicRegistry::removeTopic(TopicId id)
{
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = idToDesc_.find(id);
    if (it == idToDesc_.end()) {
        return false;
    }
    // Remove name mapping
    nameToId_.erase(it->second.name);
    // Remove descriptor
    idToDesc_.erase(it);
    return true;
}

} // namespace MotorStudio
