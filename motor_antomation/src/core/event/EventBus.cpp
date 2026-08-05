#include "EventBus.h"

namespace MotorStudio {

struct EventBus::Impl {
    // TODO: 订阅表 + 事件队列
};

EventBus& EventBus::instance() {
    static EventBus bus;
    return bus;
}

EventBus::EventBus() : d(new Impl()) {}
EventBus::~EventBus() = default;

// TODO: 实现 publish/subscribe

} // namespace MotorStudio