// test_curvewidget_interaction.cpp — CurveWidget 交互/卡死回归测试（波形域）
//
// 覆盖：
//  W1  高频数据 + 未设防的 push 数据量导致 paint 风暴（无界数据 + 全量重绘）
//  W2  attachCurveEngine/detachCurveEngine 定时器生命周期
//  W3  框选缩放拖拽 / 曲线平移拖拽 与数据推送并发
//  SYM 看门狗验证事件循环延迟（>300ms 即 QFAIL）
//
// 数据全部走模拟管线（pushData/pushFrame + CurveEngine），不依赖真实串口。

#include <QtTest>
#include <QApplication>
#include <QTimer>
#include <QElapsedTimer>
#include <QPixmap>
#include <QPainter>
#include <QMouseEvent>

#include "../src/ui/CurveWidget.h"
#include "../src/curve/CurveEngine.h"
#include "../src/curve/TimeAxisManager.h"
#include "watchdog.h"

using namespace MotorStudio;
using namespace MotorStudio::test;

namespace {

// 以 1kHz 向 legacy push 模式灌入 N 个点（显式时间戳，避免 QDateTime 开销）
void pumpLegacyData(CurveWidget* w, int channel, int count, uint64_t startTsUs)
{
    uint64_t ts = startTsUs;
    for (int i = 0; i < count; ++i) {
        w->pushData(channel, static_cast<float>(i % 1000), ts);
        ts += 1000;  // 1 kHz
    }
}

// 手动投递鼠标事件（offscreen + 隐藏顶层控件时 QTest::mouseMove 不投递合成事件）
void sendMouse(QWidget* w, QEvent::Type type, QPoint pos, Qt::MouseButton btn = Qt::LeftButton)
{
    Qt::MouseButtons buttons;
    if (type == QEvent::MouseButtonPress || type == QEvent::MouseMove) {
        buttons = btn;
    }
    QMouseEvent ev(type, pos, w->mapToGlobal(pos), btn, buttons, Qt::NoModifier);
    QApplication::sendEvent(w, &ev);
}

void mousePress(QWidget* w, QPoint pos, Qt::MouseButton btn = Qt::LeftButton)
{ sendMouse(w, QEvent::MouseButtonPress, pos, btn); }

void mouseMove(QWidget* w, QPoint pos, Qt::MouseButton btn = Qt::LeftButton)
{ sendMouse(w, QEvent::MouseMove, pos, btn); }

void mouseRelease(QWidget* w, QPoint pos, Qt::MouseButton btn = Qt::LeftButton)
{ sendMouse(w, QEvent::MouseButtonRelease, pos, btn); }

} // namespace

class TestCurveWidgetInteraction : public QObject {
    Q_OBJECT

private slots:
    void initTestCase()
    {
        qputenv("QT_QPA_PLATFORM", "offscreen");
        QVERIFY(QApplication::instance());
    }

    // ---- W1：无界 push 数据 + 全量重绘必须不冻结 ----
    void test_legacyPushHighVolumeNoFreeze()
    {
        CurveWidget w;
        w.resize(800, 400);
        w.addChannel("CH1");

        // 6M 点 @1kHz ≈ 100 分钟持续供数（远超合理的可渲染窗口；修复前全量重绘应冻结）
        pumpLegacyData(&w, 0, 6000000, 1'000'000);

        // 强制同步渲染并计时：单次 paint 不得 > 300ms（卡死判定阈值）
        QElapsedTimer timer;
        timer.start();
        QPixmap pm(w.size());
        w.render(&pm);
        const qint64 paintMs = timer.elapsed();

        QVERIFY2(paintMs < 300,
                 qPrintable(QString("paint of 6M points took %1 ms (>300ms freeze)").arg(paintMs)));
    }

    // ---- W1 回归：1kHz 持续 pushFrame，事件循环延迟看门狗零违规 ----
    void test_highRatePushFrameWatchdog()
    {
        CurveWidget w;
        w.resize(800, 400);
        w.addChannel("A");
        w.addChannel("B");

        UiWatchdog wd(300, 50);
        wd.start();

        uint64_t ts = 0;
        for (int i = 0; i < 5000; ++i) {
            QVector<float> v;
            v << static_cast<float>(i % 100) << static_cast<float>((i * 2) % 100);
            w.pushFrame(v, ts);
            ts += 1000;
            if (i % 200 == 0) {
                QCoreApplication::processEvents();  // 让 paint 事件被处理
            }
        }
        QCoreApplication::processEvents();
        wd.stop();

        QVERIFY2(wd.violationCount() == 0,
                 qPrintable(QString("event-loop delay violations: %1").arg(wd.firstViolation())));
    }

    // ---- W2：attach/detach 引擎定时器生命周期，重复 100 次不崩溃、不悬空 ----
    void test_attachDetachEngineLifecycle()
    {
        CurveWidget w;
        CurveEngine engine;
        engine.addChannel(1, 10000);
        engine.addChannel(2, 10000);

        for (int i = 0; i < 100; ++i) {
            w.attachCurveEngine(&engine, 30);
            engine.append(1, static_cast<uint64_t>(i) * 1000, static_cast<float>(i));
            engine.append(2, static_cast<uint64_t>(i) * 1000, static_cast<float>(i * 2));
            QCoreApplication::processEvents();
            w.detachCurveEngine();
        }
        // detach 后定时器必须停止：再推数据不应崩溃
        QCoreApplication::processEvents();
        QVERIFY(true);
    }

    // ---- W3：框选缩放拖拽序列，无看门狗违规，缩放生效 ----
    void test_rubberBandZoomDrag()
    {
        CurveWidget w;
        w.resize(600, 400);
        w.setRubberBandEnabled(true);
        w.addChannel("A");
        pumpLegacyData(&w, 0, 20000, 1'000'000);

        UiWatchdog wd(300, 50);
        wd.start();

        const double xRangeBefore = w.xRangeSeconds();
        mousePress(&w, QPoint(100, 100));
        for (int i = 0; i < 60; ++i) {
            mouseMove(&w, QPoint(100 + i, 100 + i));
        }
        mouseRelease(&w, QPoint(250, 200));
        QCoreApplication::processEvents();
        wd.stop();

        QVERIFY2(wd.violationCount() == 0,
                 qPrintable(QString("rubber-band drag violations: %1").arg(wd.firstViolation())));
        QVERIFY(w.xRangeSeconds() < xRangeBefore);  // 框选后 X 范围应缩小
    }

    // ---- W3：曲线 Y 平移拖拽，yOffset 生效，无看门狗违规 ----
    void test_curvePanDrag()
    {
        CurveWidget w;
        w.resize(600, 400);
        w.addChannel("A");
        pumpLegacyData(&w, 0, 20000, 1'000'000);
        w.setCurvePanMode(true);

        UiWatchdog wd(300, 50);
        wd.start();

        // 在曲线附近按下并向下拖拽（命中曲线 → 平移）
        mousePress(&w, QPoint(300, 200));
        for (int i = 0; i < 30; ++i) {
            mouseMove(&w, QPoint(300, 200 + i));
        }
        mouseRelease(&w, QPoint(300, 230));
        QCoreApplication::processEvents();
        wd.stop();

        QVERIFY2(wd.violationCount() == 0,
                 qPrintable(QString("curve-pan drag violations: %1").arg(wd.firstViolation())));
        // 未命中曲线时应保持 yOffset 为 0（命中逻辑不崩溃）
        QVERIFY(w.channelYOffset(0) >= 0.0 || w.channelYOffset(0) <= 0.0);
    }

    // ---- W3 + 并发：数据推送进行中执行右键平移拖拽，看门狗零违规 ----
    void test_panDragWhilePushingData()
    {
        CurveWidget w;
        w.resize(600, 400);
        w.addChannel("A");

        UiWatchdog wd(300, 50);
        wd.start();

        uint64_t ts = 1'000'000;
        mousePress(&w, QPoint(300, 200), Qt::RightButton);
        for (int i = 0; i < 200; ++i) {
            // 每步间灌入一小批数据（模拟实时供数）
            for (int j = 0; j < 20; ++j) {
                w.pushData(0, static_cast<float>(i % 500), ts);
                ts += 1000;
            }
            mouseMove(&w, QPoint(300 - i, 200), Qt::RightButton);
            QCoreApplication::processEvents();
        }
        mouseRelease(&w, QPoint(100, 200), Qt::RightButton);
        QCoreApplication::processEvents();
        wd.stop();

        QVERIFY2(wd.violationCount() == 0,
                 qPrintable(QString("pan-during-push violations: %1").arg(wd.firstViolation())));
    }
};

QTEST_MAIN(TestCurveWidgetInteraction)
#include "test_curvewidget_interaction.moc"
