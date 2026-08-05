#include "CommandQueue.h"

namespace MotorStudio {

struct CommandQueue::Impl {
    std::queue<PendingCommand> pending;
    uint16_t nextSeqId = 0;
};

CommandQueue::CommandQueue(QObject* parent) : QObject(parent), d(std::make_unique<Impl>()) {}
CommandQueue::~CommandQueue() = default;

void CommandQueue::enqueue(const Command& cmd, std::function<void(const Response&)> callback) {
    // TODO: 入队 + 去重检查
}

void CommandQueue::onResponse(const Response& resp) {
    // TODO: 匹配 pending 命令
}

void CommandQueue::checkTimeouts() {
    // TODO: 超时重试逻辑
}

size_t CommandQueue::pendingCount() const {
    return d->pending.size();
}

} // namespace MotorStudio