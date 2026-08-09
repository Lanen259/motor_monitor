// stress_waveform_ui.cpp — 波形域 Debug 模拟仿真压力测试（夜间防卡死自测 Phase 4）
//
// 构造 MultiCurveContainer + VerticalPlotList/PlotCell + CurveEngine + DeviceSimulator，
// 以 ≥1kHz 帧率持续供数（DeviceSimulator→VofaParser→DataBus→CurveEngine 模拟管线），
// 同时用固定种子伪随机序列从操作池抽取并连续执行 ≥500 次随机交互。
// 全程挂事件循环延迟看门狗：单次事件处理 > 300ms 即 QFAIL，并输出当时操作与帧率。
//
// 3 轮不同种子全绿才算通过（QTest data-driven）。

#include <QtTest>
#include <QApplication>
#include <QTimer>
#include <QElapsedTimer>
#include <QScrollBar>
#include <random>

#include "../src/device/DeviceSimulator.h"
#include "../src/communication/protocol/VofaParser.h"
#include "../src/databus/DataBus.h"
#include "../src/databus/Topic.h"
#include "../src/curve/CurveEngine.h"
#include "../src/ui/MultiCurveContainer.h"
#include "../src/ui/VerticalPlotList.h"
#include "../src/ui/PlotCell.h"
#include "../src/ui/CurveWidget.h"
#include "../src/curve/TimeAxisManager.h"
#include "watchdog.h"

using namespace MotorStudio;
using namespace MotorStudio::test;

namespace {

// 12 通道 JustFloat 序列化（与 test_mock_mcu_curve 一致）
QByteArray serializeJustFloat(const MotorDataPayload& p)
{
    float fields[12];
    fields[0]  = static_cast<float>(p.timestamp);
    fields[1]  = p.ia; fields[2]  = p.ib; fields[3]  = p.ic;
    fields[4]  = p.id; fields[5]  = p.iq; fields[6]  = p.speed;
    fields[7]  = p.position; fields[8] = p.busVoltage;
    fields[9]  = p.busCurrent; fields[10] = p.temperature;
    fields[11] = static_cast<float>(p.fault);
    return QByteArray(reinterpret_cast<const char*>(fields), static_cast<int>(sizeof(fields)));
}

// 手动投递鼠标事件（offscreen + 顶层隐藏时不走真实窗口事件）
void sendMouse(QWidget* w, QEvent::Type type, QPoint pos, Qt::MouseButton btn)
{
    Qt::MouseButtons buttons = (type == QEvent::MouseButtonRelease) ? Qt::NoButton : btn;
    QMouseEvent ev(type, pos, w->mapToGlobal(pos), btn, buttons, Qt::NoModifier);
    QApplication::sendEvent(w, &ev);
}

// 给 CurveWidget 配置指定通道（关闭 auto-populate，模拟"子图只显示选中通道"）
void configureWidgetChannels(CurveWidget* cw, const std::vector<uint32_t>& topics, int perWidget)
{
    if (!cw) return;
    cw->setAutoPopulateChannels(false);
    for (int t = 0; t < perWidget; ++t) {
        uint32_t tid = topics[1 + static_cast<size_t>(t) % (topics.size() - 1)];
        int idx = cw->addChannel(QString("CH%1").arg(tid));
        cw->setChannelTopicId(idx, tid);
    }
}

// 记录操作描述，供看门狗违规时输出上下文
struct OpLog {
    QStringList ops;
    void add(const QString& s) { ops.append(s); }
    QString lastN(int n = 5) const {
        QStringList t = ops.mid(qMax(0, ops.size() - n));
        return t.join(" -> ");
    }
};

} // namespace

class StressWaveformUi : public QObject {
    Q_OBJECT

private slots:
    void randomInteraction_data()
    {
        QTest::addColumn<int>("seed");
        QTest::newRow("seed-1") << 12345;
        QTest::newRow("seed-2") << 67890;
        QTest::newRow("seed-3") << 24680;
    }

    void randomInteraction()
    {
        QFETCH(int, seed);

        // ==== 数据管线：DeviceSimulator(1kHz) -> VofaParser -> DataBus -> CurveEngine ====
        DeviceSimulator sim;
        sim.setFrequency(1000);          // ≥1kHz
        sim.setNominalSpeed(3000.0f);
        sim.setNominalVoltage(24.0f);

        VofaParser vofa;
        vofa.setProtocolType(VofaParser::JustFloat);
        vofa.setChannelCount(12);

        const std::vector<uint32_t> topics = {
            Topics::Timestamp, Topics::Ia, Topics::Ib, Topics::Ic, Topics::Id, Topics::Iq,
            Topics::Speed, Topics::Position, Topics::Voltage, Topics::Current,
            Topics::Temperature, Topics::Fault
        };

        CurveEngine engine;
        for (uint32_t tid : topics) engine.addChannel(tid, 100000);

        QObject::connect(&sim, &DeviceSimulator::dataGenerated,
                         [&vofa](const MotorDataPayload& payload) {
                             vofa.feed(serializeJustFloat(payload));
                         });
        auto& dataBus = DataBus::instance();
        QObject::connect(&vofa, &VofaParser::frameParsed,
                         [&dataBus, &topics](const QVector<float>& values) {
                             uint64_t tsUs = static_cast<uint64_t>(values[0]) * 1000ULL;
                             dataBus.publishFrame(topics, values, tsUs);
                         });
        for (uint32_t tid : topics) {
            dataBus.subscribe(tid, [&engine](const DataPoint& point) {
                engine.append(point);
            });
        }

        // ==== 控件 ====
        MultiCurveContainer container;
        container.resize(1200, 800);
        container.attachCurveEngine(&engine, 15);
        for (int c = 0; c < container.curveWidgetCount(); ++c) {
            configureWidgetChannels(container.curveWidgetAt(c), topics, 3);
        }

        VerticalPlotList plotList(&engine);
        plotList.resize(1200, 800);
        for (int i = 0; i < 2; ++i) plotList.addPlot(QString("Plot%1").arg(i));
        for (PlotCell* cell : plotList.plotWidgets()) {
            if (cell && cell->curveWidget()) {
                cell->curveWidget()->setAutoPopulateChannels(true);
                cell->curveWidget()->attachCurveEngine(&engine, 15);
            }
        }
        for (int p = 0; p < plotList.plotCount(); ++p) {
            if (PlotCell* cell = plotList.plotAt(p)) {
                QVector<uint32_t> chs;
                for (int t = 0; t < 2; ++t) chs.append(topics[1 + (p * 2 + t) % 11]);
                cell->setChannels(chs);
            }
        }

        sim.start();

        // ==== 随机交互主循环 ====
        std::mt19937 rng(static_cast<uint32_t>(seed));
        UiWatchdog wd(300, 50);
        wd.start();
        OpLog log;
        const int OPS = 500;

        QElapsedTimer totalTimer;
        totalTimer.start();
        uint64_t lastFrameCount = 0;

        for (int i = 0; i < OPS; ++i) {
            const int op = static_cast<int>(rng() % 14);
            const int cwCount = container.curveWidgetCount();
            const int plotCount = plotList.plotCount();

            switch (op) {
            case 0:  // 模式切换
                container.setMode((i % 2 == 0) ? MultiCurveContainer::GridMode
                                               : MultiCurveContainer::TabMode);
                log.add("setMode");
                break;
            case 1: {  // 网格尺寸（上限 3x2，控制渲染负载）
                container.setGridSize(2, 2);
                log.add("setGridSize");
                break;
            }
            case 2:  // 新增标签
                container.addTab(QString("T%1").arg(i));
                log.add("addTab");
                break;
            case 3:  // 删除标签
                if (container.mode() == MultiCurveContainer::TabMode && cwCount > 1) {
                    container.removeTab(cwCount - 1);
                    log.add("removeTab");
                } else {
                    container.removeTab(0);
                    log.add("removeTab(0)");
                }
                break;
            case 4: {  // 单元格右键平移拖拽
                if (cwCount > 0) {
                    CurveWidget* cw = container.curveWidgetAt(static_cast<int>(rng() % cwCount));
                    if (cw) {
                        QPoint start(300 + static_cast<int>(rng() % 150), 180 + static_cast<int>(rng() % 80));
                        sendMouse(cw, QEvent::MouseButtonPress, start, Qt::RightButton);
                        for (int m = 0; m < 12; ++m) {
                            sendMouse(cw, QEvent::MouseMove, start + QPoint(-m, 0), Qt::RightButton);
                            if (m % 3 == 2) QCoreApplication::processEvents(QEventLoop::AllEvents);
                        }
                        sendMouse(cw, QEvent::MouseButtonRelease, start + QPoint(-12, 0), Qt::RightButton);
                        log.add("panDrag");
                    }
                }
                break;
            }
            case 5: {  // 单元格滚轮缩放
                if (cwCount > 0) {
                    CurveWidget* cw = container.curveWidgetAt(static_cast<int>(rng() % cwCount));
                    if (cw) {
                        QWheelEvent we(QPoint(300, 200), cw->mapToGlobal(QPoint(300, 200)),
                                       QPoint(), QPoint(0, (rng() % 2 ? 120 : -120)),
                                       Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false);
                        QApplication::sendEvent(cw, &we);
                        log.add("wheel");
                    }
                }
                break;
            }
            case 6: {  // 曲线 Y 偏移平移
                if (cwCount > 0) {
                    CurveWidget* cw = container.curveWidgetAt(static_cast<int>(rng() % cwCount));
                    if (cw) {
                        cw->setCurvePanMode(true);
                        QPoint start(300, 200);
                        sendMouse(cw, QEvent::MouseButtonPress, start, Qt::LeftButton);
                        for (int m = 0; m < 10; ++m) {
                            sendMouse(cw, QEvent::MouseMove, QPoint(300, 200 + m), Qt::LeftButton);
                            if (m % 2 == 1) QCoreApplication::processEvents(QEventLoop::AllEvents);
                        }
                        sendMouse(cw, QEvent::MouseButtonRelease, QPoint(300, 210), Qt::LeftButton);
                        cw->setCurvePanMode(false);
                        log.add("curvePan");
                    }
                }
                break;
            }
            case 7: {  // 橡皮筋框选缩放
                if (cwCount > 0) {
                    CurveWidget* cw = container.curveWidgetAt(static_cast<int>(rng() % cwCount));
                    if (cw) {
                        cw->setRubberBandEnabled(true);
                        QPoint start(100, 100);
                        sendMouse(cw, QEvent::MouseButtonPress, start, Qt::LeftButton);
                        for (int m = 0; m < 12; ++m) {
                            sendMouse(cw, QEvent::MouseMove, start + QPoint(m, m), Qt::LeftButton);
                            if (m % 3 == 2) QCoreApplication::processEvents(QEventLoop::AllEvents);
                        }
                        sendMouse(cw, QEvent::MouseButtonRelease, start + QPoint(12, 12), Qt::LeftButton);
                        cw->setRubberBandEnabled(false);
                        log.add("rubberBand");
                    }
                }
                break;
            }
            case 8:  // 新增子图
                plotList.addPlot(QString("P%1").arg(i));
                if (PlotCell* cell = plotList.plotAt(plotList.plotCount() - 1)) {
                    QVector<uint32_t> chs;
                    for (int t = 0; t < 2; ++t) chs.append(topics[1 + static_cast<int>(rng() % 11)]);
                    cell->setChannels(chs);
                }
                log.add("addPlot");
                break;
            case 9:  // 删除子图
                if (plotCount > 1) {
                    plotList.removePlot(static_cast<int>(rng() % plotCount));
                    log.add("removePlot");
                }
                break;
            case 10: {  // 子图列表拖拽滚动
                QPoint start(600, 200);
                sendMouse(plotList.viewport(), QEvent::MouseButtonPress, start, Qt::LeftButton);
                for (int m = 0; m < 10; ++m) {
                    sendMouse(plotList.viewport(), QEvent::MouseMove,
                              QPoint(600, 200 - m * 2), Qt::LeftButton);
                    if (m % 3 == 2) QCoreApplication::processEvents(QEventLoop::AllEvents);
                }
                sendMouse(plotList.viewport(), QEvent::MouseButtonRelease,
                          QPoint(600, 200 - 20), Qt::LeftButton);
                log.add("dragScroll");
                break;
            }
            case 11: {  // 橡皮筋模式切换
                bool rb = (rng() % 2 == 0);
                for (PlotCell* cell : plotList.plotWidgets()) {
                    if (cell && cell->curveWidget()) cell->curveWidget()->setRubberBandEnabled(rb);
                }
                log.add("toggleRubberBand");
                break;
            }
            case 12: {  // PlotCell 通道增删
                if (plotCount > 0) {
                    PlotCell* cell = plotList.plotAt(static_cast<int>(rng() % plotCount));
                    if (cell) {
                        uint32_t tid = topics[1 + static_cast<int>(rng() % 11)];
                        if (rng() % 2) cell->addChannel(tid);
                        else cell->removeChannel(tid);
                        log.add("channelAddRemove");
                    }
                }
                break;
            }
            case 13:  // 直推/拉取模式交替
                if (cwCount > 0) {
                    CurveWidget* cw = container.curveWidgetAt(static_cast<int>(rng() % cwCount));
                    if (cw) {
                        if (rng() % 2) cw->detachCurveEngine();
                        else cw->attachCurveEngine(&engine, 15);
                        log.add("modeToggle");
                    }
                }
                break;
            default:
                break;
            }

            // 每步间 0~5ms 随机延迟
            int delayMs = static_cast<int>(rng() % 6);
            if (delayMs > 0) QTest::qWait(delayMs);
            QCoreApplication::processEvents(QEventLoop::AllEvents);

            // 定期检查帧率是否持续供数（防止数据管线挂死）
            if (i % 100 == 0) {
                uint64_t now = sim.framesGenerated();
                if (i > 0) {
                    QVERIFY2(now > lastFrameCount,
                             qPrintable(QString("simulator stalled at op %1").arg(i)));
                }
                lastFrameCount = now;
            }
        }

        wd.stop();
        sim.stop();

        const qint64 totalMs = totalTimer.elapsed();
        QVERIFY2(wd.violationCount() == 0,
                 qPrintable(QString("watchdog violations: %1 | context: %2 | frames=%3")
                            .arg(wd.firstViolation()).arg(log.lastN()).arg(sim.framesGenerated())));

        QVERIFY2(totalMs < 120000, qPrintable(QString("stress took %1 ms (too slow)").arg(totalMs)));
    }
};

QTEST_MAIN(StressWaveformUi)
#include "stress_waveform_ui.moc"
