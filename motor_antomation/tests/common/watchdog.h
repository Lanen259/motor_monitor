#pragma once

// ============================================================
// UiWatchdog — 事件循环延迟看门狗（三域共享规格 §2.2）
// 后台线程每 50ms 向 UI 线程投递一次心跳（QueuedConnection），
// 记录投递时刻（t_posted）与执行时刻（t_exec），差 > thresholdMs
// 即记为一次"卡死"违规。另用"相邻两次心跳执行的墙钟间隔"做第二
// 信号（catch 事件队列堆积时单次差偏小的场景）。
// 用例结束前必须 violations().isEmpty()，否则 QFAIL。
// ============================================================

#include <QObject>
#include <QThread>
#include <QAtomicInt>
#include <QStringList>
#include <QMutex>
#include <chrono>

class UiWatchdog : public QObject {
public:
    explicit UiWatchdog(int thresholdMs = 300, QObject* parent = nullptr)
        : QObject(parent), m_thresholdMs(thresholdMs) {}

    // 启动心跳线程；uiContext 必须存活于 UI 线程（测试主线程）。
    void start(QObject* uiContext)
    {
        if (m_thread) return;
        m_uiContext = uiContext;
        m_stop.storeRelaxed(0);

        m_thread = new QThread();
        m_thread->setObjectName(QStringLiteral("UiWatchdogHeartbeat"));

        auto* beat = new QObject();
        beat->moveToThread(m_thread);

        connect(m_thread, &QThread::started, beat, [this]() {
            while (!m_stop.loadRelaxed()) {
                const qint64 posted = nowMs();
                QMetaObject::invokeMethod(m_uiContext, [this, posted]() {
                    onBeat(posted);
                }, Qt::QueuedConnection);
                // 心跳周期 50ms（后台线程，msleep 合法，不受 UI 线程阻塞影响）
                for (int i = 0; i < 50 && !m_stop.loadRelaxed(); i += 5)
                    QThread::msleep(5);
            }
        });

        m_thread->start();
    }

    void stop()
    {
        if (!m_thread) return;
        m_stop.storeRelaxed(1);
        m_thread->quit();
        m_thread->wait(2000);
        delete m_thread;
        m_thread = nullptr;
    }

    QStringList violations() const
    {
        QMutexLocker lk(&m_mutex);
        return m_violations;
    }

    int violationCount() const
    {
        QMutexLocker lk(&m_mutex);
        return m_violations.size();
    }

    int thresholdMs() const { return m_thresholdMs; }

private:
    void onBeat(qint64 postedMs)
    {
        const qint64 now = nowMs();
        const qint64 latency = now - postedMs;
        const qint64 gap = (m_lastBeatMs >= 0) ? (now - m_lastBeatMs) : 0;

        if (latency > m_thresholdMs) {
            QMutexLocker lk(&m_mutex);
            m_violations.append(QStringLiteral("UI stall: heartbeat latency %1 ms > %2 ms")
                .arg(latency).arg(m_thresholdMs));
        } else if (gap > m_thresholdMs) {
            QMutexLocker lk(&m_mutex);
            m_violations.append(QStringLiteral("UI stall: heartbeat gap %1 ms > %2 ms")
                .arg(gap).arg(m_thresholdMs));
        }
        m_lastBeatMs = now;
    }

private:
    static qint64 nowMs()
    {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
    }

    int m_thresholdMs = 300;
    QThread* m_thread = nullptr;
    QObject* m_uiContext = nullptr;
    QAtomicInt m_stop;
    mutable QMutex m_mutex;
    QStringList m_violations;
    qint64 m_lastBeatMs = -1;
};
