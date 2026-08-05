#pragma once
#include <QObject>
#include <queue>
#include <memory>
#include <chrono>
#include <functional>
#include "../../core/message/Message.h"

namespace MotorStudio {

// 待处理命令
struct PendingCommand {
    Command cmd;
    std::chrono::steady_clock::time_point sendTime;
    int retryCount = 0;
    std::function<void(const Response&)> callback;
};

// 异步命令队列
class CommandQueue : public QObject {
    Q_OBJECT
public:
    explicit CommandQueue(QObject* parent = nullptr);
    ~CommandQueue() override;

    // 入队命令
    void enqueue(const Command& cmd, std::function<void(const Response&)> callback);

    // 处理响应
    void onResponse(const Response& resp);

    // 超时检查
    void checkTimeouts();

    size_t pendingCount() const;

signals:
    void commandReady(const Command& cmd);
    void commandTimeout(uint16_t cmdId);

private:
    struct Impl;
    std::unique_ptr<Impl> d;
};

} // namespace MotorStudio