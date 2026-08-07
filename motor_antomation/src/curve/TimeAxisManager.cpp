#include "TimeAxisManager.h"
#include <QMutexLocker>

namespace MotorStudio {

// ============================================================
// 单例
// ============================================================

TimeAxisManager& TimeAxisManager::instance()
{
    static TimeAxisManager s_instance;
    return s_instance;
}

// ============================================================
// 共享范围访问
// ============================================================

TimeRange TimeAxisManager::sharedRange() const
{
    QMutexLocker locker(&m_mutex);
    return m_sharedRange;
}

// ============================================================
// 更新共享范围（原子操作，避免信号风暴）
// ============================================================

void TimeAxisManager::updateSharedRange(uint64_t t0, double xRangeSeconds)
{
    // 在锁内更新数据
    {
        QMutexLocker locker(&m_mutex);
        m_sharedRange.t0 = t0;
        m_sharedRange.xRangeSeconds = xRangeSeconds;
    }

    // 在锁外发射信号和通知监听器，避免死锁
    // 对监听器列表加锁以安全遍历
    QMap<ListenerId, std::function<void(uint64_t, double)>> listenersCopy;
    {
        QMutexLocker locker(&m_mutex);
        listenersCopy = m_listeners;
    }

    // 发射 Qt 信号（只发射一次）
    emit sharedRangeChanged(t0, xRangeSeconds);

    // 通知所有监听器
    for (auto it = listenersCopy.begin(); it != listenersCopy.end(); ++it) {
        if (it.value()) {
            it.value()(t0, xRangeSeconds);
        }
    }
}

// ============================================================
// 监听器注册/取消注册
// ============================================================

TimeAxisManager::ListenerId TimeAxisManager::registerListener(
    std::function<void(uint64_t, double)> callback)
{
    QMutexLocker locker(&m_mutex);
    ListenerId id = m_nextId++;
    m_listeners.insert(id, callback);
    return id;
}

void TimeAxisManager::unregisterListener(ListenerId id)
{
    QMutexLocker locker(&m_mutex);
    m_listeners.remove(id);
}

} // namespace MotorStudio
