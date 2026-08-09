// test_plotcell_interaction.cpp — PlotCell 交互/重建风暴回归测试（波形域）
//
// 覆盖：
//  WF-03 P4  setChannels 逐通道 rebuildChannelBar O(N²) 控件churn
//  SYM        名称编辑(键入→Enter)→ 立即拖动曲线区（看门狗验证）
//  W4        通道增删与供数并发

#include <QtTest>
#include <QApplication>
#include <QLineEdit>
#include <QCheckBox>

#include "../src/ui/PlotCell.h"
#include "../src/ui/CurveWidget.h"
#include "../src/curve/CurveEngine.h"
#include "../src/databus/Topic.h"
#include "watchdog.h"

using namespace MotorStudio;
using namespace MotorStudio::test;

class TestPlotCellInteraction : public QObject {
    Q_OBJECT

private slots:
    void initTestCase()
    {
        qputenv("QT_QPA_PLATFORM", "offscreen");
        QVERIFY(QApplication::instance());
        // 注册测试用 topic（避免污染全局注册表，用大 ID 段）
    }

    // ---- WF-03 P4：setChannels 大量通道不得 O(N²) 重建风暴 ----
    void test_setChannelsManyNoRebuildStorm()
    {
        CurveEngine engine;
        PlotCell cell("P1", &engine);
        cell.resize(600, 400);
        cell.show();
        QVERIFY(QTest::qWaitForWindowExposed(&cell));

        auto& reg = TopicRegistry::instance();
        QVector<uint32_t> topicIds;
        for (int i = 0; i < 60; ++i) {
            uint32_t tid = reg.registerTopic(QString("TestCh%1").arg(i).toStdString());
            topicIds.append(tid);
            engine.addChannel(tid, 10000);
        }

        UiWatchdog wd(300, 50);
        wd.start();

        // 先清空再设置全部通道（内部逐通道 addChannel + rebuildChannelBar）
        cell.setChannels(topicIds);
        QCoreApplication::processEvents();
        QVERIFY(cell.channels().size() == topicIds.size());

        QCoreApplication::processEvents();
        wd.stop();

        QVERIFY2(wd.violationCount() == 0,
                 qPrintable(QString("setChannels rebuild violations: %1").arg(wd.firstViolation())));
    }

    // ---- SYM：名称编辑(键入→Enter) → 立即拖动曲线区，看门狗零违规 ----
    void test_nameEditEnterThenImmediateDrag()
    {
        CurveEngine engine;
        PlotCell cell("P1", &engine);
        cell.resize(600, 400);
        cell.show();
        QVERIFY(QTest::qWaitForWindowExposed(&cell));

        // 模拟双击名称标签进入编辑（或直接驱动隐藏的 QLineEdit）
        auto* nameEdit = cell.findChild<QLineEdit*>();
        QVERIFY(nameEdit);

        UiWatchdog wd(300, 50);
        wd.start();

        // 键入 → Enter（触发 editingFinished）→ 立即拖动曲线区
        nameEdit->setFocus();
        nameEdit->setText("Renamed");
        QTest::keyClick(nameEdit, Qt::Key_Enter);
        QCoreApplication::processEvents();

        CurveWidget* cw = cell.curveWidget();
        QVERIFY(cw);
        QTest::mousePress(cw, Qt::RightButton, {}, QPoint(300, 200));
        for (int i = 0; i < 40; ++i) {
            QTest::mouseMove(cw, QPoint(300 - i, 200));
        }
        QTest::mouseRelease(cw, Qt::RightButton, {}, QPoint(260, 200));
        QCoreApplication::processEvents();
        wd.stop();

        QVERIFY2(wd.violationCount() == 0,
                 qPrintable(QString("SYM nameEdit drag violations: %1").arg(wd.firstViolation())));
        QCOMPARE(cell.name(), QString("Renamed"));
    }

    // ---- W4：通道增删 + 供数并发，重复 200 次不崩溃 ----
    void test_channelAddRemoveWithData()
    {
        CurveEngine engine;
        PlotCell cell("P1", &engine);
        cell.resize(600, 400);
        cell.show();
        QVERIFY(QTest::qWaitForWindowExposed(&cell));

        auto& reg = TopicRegistry::instance();
        QVector<uint32_t> topicIds;
        for (int i = 0; i < 8; ++i) {
            uint32_t tid = reg.registerTopic(QString("DynCh%1").arg(i).toStdString());
            topicIds.append(tid);
            engine.addChannel(tid, 20000);
        }

        UiWatchdog wd(300, 50);
        wd.start();

        uint64_t ts = 1'000'000;
        for (int i = 0; i < 200; ++i) {
            // 增删一个通道
            uint32_t tid = topicIds[i % topicIds.size()];
            if (i % 2 == 0) {
                cell.addChannel(tid);
            } else {
                cell.removeChannel(tid);
            }
            // 供数
            for (uint32_t t : topicIds) {
                engine.append(t, ts, static_cast<float>(i % 300));
            }
            ts += 1000;
            QCoreApplication::processEvents();
        }
        QCoreApplication::processEvents();
        wd.stop();

        QVERIFY2(wd.violationCount() == 0,
                 qPrintable(QString("channel churn violations: %1").arg(wd.firstViolation())));
    }
};

QTEST_MAIN(TestPlotCellInteraction)
#include "test_plotcell_interaction.moc"
