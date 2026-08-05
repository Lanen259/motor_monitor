#pragma once
#include <QObject>
#include <functional>
#include <memory>
#include <typeindex>
#include <unordered_map>
#include <vector>

namespace MotorStudio {

// 事件基类
struct Event {
    virtual ~Event() = default;
};

// 事件总线（单例，跨模块通信）
class EventBus : public QObject {
    Q_OBJECT
public:
    static EventBus& instance();

    ~EventBus() override;

    template<typename T, typename Func>
    void subscribe(Func&& callback);

    template<typename T>
    void publish(const T& event);

signals:
    void eventPublished(const std::type_index& type);

private:
    EventBus();
    struct Impl;
    std::unique_ptr<Impl> d;
};

} // namespace MotorStudio