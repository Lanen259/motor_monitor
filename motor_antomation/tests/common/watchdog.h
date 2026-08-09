// watchdog.h — 事件循环延迟看门狗（三域共享实现参考，见《夜间防卡死自测_共享执行规格》§2.2）
//
// 判定标准：UI 线程事件循环单次事件处理 > thresholdMs(默认 300ms) 即记录一次 violation。
// 原理：后台线程每 heartbeatMs(默认 50ms) 记录 t0，并向本对象（UI 线程）投递一个
//        QueuedConnection 心跳 lambda；心跳在 UI 线程事件循环执行时取 t1，若 t1-t0 > 阈值
//        → 记一条违规（含时间戳）。UI 线程被某事件阻塞越久，心跳延迟越大，即被捕获。
// 用法（QTest）：
//   UiWatchdog wd; wd.start();
//   ... 跑交互/压力 ...
//   wd.stop();
//   QVERIFY2(wd.violationCount() == 0, wd.firstViolation().toUtf8().constData());
//
// 头文件自包含，无 Q_OBJECT（使用 QMetaObject::invokeMethod(context, functor, QueuedConnection)），
// 仅需 Qt5::Core。本文件属于波形域 tests/ 公共设施。

#pragma once

#include <QObject>
#include <QStringList>
#include <QMetaObject>
#include <QThread>
#include <atomic>
#include <thread>
#include <chrono>

namespace MotorStudio {
namespace test {

class UiWatchdog : public QObject {
public:
    explicit UiWatchdog(int thresholdMs = 300, int heartbeatMs = 50, QObject* parent = nullptr)
        : QObject(parent)
        , m_thresholdMs(thresholdMs)
        , m_heartbeatMs(heartbeatMs)
    {
        // 看门狗对象必须归属 UI 线程（构造于测试主线程即可）。
    }

    ~UiWatchdog() override { stop(); }

    void start()
    {
        if (m_running.exchange(true)) return;
        m_violations.clear();
        m_thread = std::thread([this]() { runLoop(); });
    }

    void stop()
    {
        if (!m_running.exchange(false)) return;
        if (m_thread.joinable()) m_thread.join();
    }

    int violationCount() const { return m_violations.size(); }
    QStringList violations() const { return m_violations; }

    QString firstViolation() const
    {
        return m_violations.isEmpty() ? QString() : m_violations.first();
    }

private:
    void runLoop()
    {
        using clock = std::chrono::steady_clock;
        while (m_running.load()) {
            const auto t0 = clock::now();
            const qint64 t0Us = std::chrono::duration_cast<std::chrono::microseconds>(
                                    t0.time_since_epoch()).count();
            // 向 UI 线程投递心跳。UI 线程若正被某事件阻塞，此心跳排队到该事件处理完才执行。
            QMetaObject::invokeMethod(this, [this, t0Us]() {
                const auto t1 = std::chrono::steady_clock::now();
                const qint64 t1Us = std::chrono::duration_cast<std::chrono::microseconds>(
                                        t1.time_since_epoch()).count();
                const qint64 delayUs = t1Us - t0Us;
                if (delayUs > static_cast<qint64>(m_thresholdMs) * 1000) {
                    const qint64 delayMs = delayUs / 1000;
                    m_violations.append(
                        QString("UI event-loop delay %1 ms (> %2 ms threshold)")
                            .arg(delayMs).arg(m_thresholdMs));
                }
            }, Qt::QueuedConnection);

            // 分片睡眠以保证 stop() 能在 ~1 个心跳周期内完成 join
            for (int slept = 0; slept < m_heartbeatMs && m_running.load(); slept += 5) {
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
            }
        }
    }

    int m_thresholdMs;
    int m_heartbeatMs;
    std::atomic<bool> m_running{false};
    std::thread m_thread;
    QStringList m_violations;  // 仅在 UI 线程追加，stop() 后由 UI 线程读取，无竞争
};

} // namespace test
} // namespace MotorStudio
