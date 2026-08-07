#pragma once

#include <QObject>
#include <QMutex>
#include <QMap>
#include <functional>
#include <cstdint>

namespace MotorStudio {

/**
 * @brief 共享时间范围结构体
 *
 * 所有同步的子图共享同一个时间基准和 X 轴范围。
 */
struct TimeRange {
    uint64_t t0 = 0;               // 时间基准（微秒）
    double xRangeSeconds = 10.0;   // X 轴显示范围（秒）
};

/**
 * @brief 共享时间轴管理器（单例）
 *
 * 默认情况下所有子图共享同一时间轴。
 * 每个 PlotCell 可通过 sync 标志切换到独立模式。
 *
 * 同步模式（sync=true）：
 *   - 缩放/平移时写入共享 TimeRange
 *   - 接收共享 TimeRange 变更通知
 *
 * 独立模式（sync=false）：
 *   - 使用本地 TimeRange，不参与同步
 *
 * 线程安全：支持跨线程访问（例如 DataBus 线程更新）。
 */
class TimeAxisManager : public QObject {
    Q_OBJECT

public:
    /**
     * @brief 获取全局单例实例
     */
    static TimeAxisManager& instance();

    /**
     * @brief 获取当前共享时间范围
     */
    TimeRange sharedRange() const;

    /**
     * @brief 更新共享时间范围
     *
     * 由同步的 PlotCell 在缩放/平移时调用。
     * 更新后会发射 sharedRangeChanged 信号并通知所有监听器。
     * 原子操作，避免信号风暴。
     *
     * @param t0 时间基准（微秒）
     * @param xRangeSeconds X 轴显示范围（秒）
     */
    void updateSharedRange(uint64_t t0, double xRangeSeconds);

    /**
     * @brief 监听器 ID 类型
     */
    using ListenerId = int;

    /**
     * @brief 注册共享范围变更监听器
     *
     * @param callback 变更回调，参数为 (t0_microseconds, xRangeSeconds)
     * @return ListenerId 用于取消注册
     */
    ListenerId registerListener(std::function<void(uint64_t, double)> callback);

    /**
     * @brief 取消注册监听器
     *
     * @param id 由 registerListener 返回的监听器 ID
     */
    void unregisterListener(ListenerId id);

signals:
    /**
     * @brief 共享时间范围变更信号
     *
     * 在一次更新中只发射一次（原子操作）。
     *
     * @param t0 时间基准（微秒）
     * @param xRangeSeconds X 轴显示范围（秒）
     */
    void sharedRangeChanged(uint64_t t0, double xRangeSeconds);

private:
    TimeAxisManager() = default;
    ~TimeAxisManager() override = default;
    TimeAxisManager(const TimeAxisManager&) = delete;
    TimeAxisManager& operator=(const TimeAxisManager&) = delete;

    mutable QMutex m_mutex;
    TimeRange m_sharedRange;
    ListenerId m_nextId = 1;
    QMap<ListenerId, std::function<void(uint64_t, double)>> m_listeners;
};

} // namespace MotorStudio
