// test_historyreplay_interaction.cpp — HistoryReplayWidget 卡死/重建风暴回归测试（波形域）
//
// 覆盖：
//  WF-07 P5  CSV 加载在 GUI 线程同步 parse+merge+sort+push，无分片/无 processEvents
//  WF-08 P4  通道过滤切换无条件全量 rebuild（clearAllChannels + 重新 push 全部数据）
//  WF-09 P2  clearAll 重填下拉触发重入 refreshCurveDisplay
//  SYM    键入输入→Enter→立即拖动 序列（看门狗验证）
//
// 数据：测试内生成临时 CSV，走 loadCSV/refreshCurveDisplay 真实路径，不依赖真实串口。

#include <QtTest>
#include <QApplication>
#include <QElapsedTimer>
#include <QTemporaryDir>
#include <QFile>
#include <QTextStream>
#include <QComboBox>

// 访问 HistoryReplayWidget 私有成员（loadCSV/refreshCurveDisplay/m_channelFilter），
// 仅测试用途：在包含本 widget 头前放开 private。
#include "watchdog.h"
#define private public
#include "../src/ui/HistoryReplayWidget.h"
#undef private
#include "../src/ui/CurveWidget.h"  // 完整类型：使 CurveWidget* 可转 QWidget*

using namespace MotorStudio;
using namespace MotorStudio::test;

namespace {

// 生成 N 行 x C 列的模拟 CSV（1 列时间戳 + C-1 列数据）
void makeCsv(const QString& path, int rows, int channels)
{
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) return;
    QTextStream out(&f);
    out << "timestamp";
    for (int c = 1; c < channels; ++c) out << ",CH" << c;
    out << "\n";
    for (int i = 0; i < rows; ++i) {
        out << (i * 1000.0);
        for (int c = 1; c < channels; ++c) out << ',' << (i % 1000);
        out << "\n";
    }
    f.close();
}

} // namespace

class TestHistoryReplayInteraction : public QObject {
    Q_OBJECT

private slots:
    void initTestCase()
    {
        qputenv("QT_QPA_PLATFORM", "offscreen");
        QVERIFY(QApplication::instance());
    }

    // ---- WF-07 P5：大 CSV 同步加载不得阻塞 GUI 线程 > 300ms ----
    void test_largeCsvLoadDoesNotBlockUiThread()
    {
        HistoryReplayWidget w;
        w.resize(800, 400);
        w.show();
        QVERIFY(QTest::qWaitForWindowExposed(&w));

        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        const QString csv = tmp.filePath("big.csv");
        // 100k 行 x 9 列 ≈ 90 万值（模拟大文件历史数据）
        makeCsv(csv, 100000, 9);

        UiWatchdog wd(300, 50);
        wd.start();
        QElapsedTimer timer;
        timer.start();
        w.loadCSV(csv);
        const qint64 loadMs = timer.elapsed();
        QCoreApplication::processEvents();
        wd.stop();

        QVERIFY2(wd.violationCount() == 0,
                 qPrintable(QString("CSV load blocked UI thread: %1").arg(wd.firstViolation())));
        QVERIFY2(loadMs < 3000,
                 qPrintable(QString("CSV load took %1 ms total").arg(loadMs)));
    }

    // ---- WF-08 P4：通道过滤切换不应触发全量 rebuild 风暴 ----
    void test_channelFilterChangeNoFullRebuildStorm()
    {
        HistoryReplayWidget w;
        w.resize(800, 400);
        w.show();
        QVERIFY(QTest::qWaitForWindowExposed(&w));

        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        const QString csv = tmp.filePath("filter.csv");
        makeCsv(csv, 50000, 6);
        w.loadCSV(csv);

        // 连续切换过滤几次，每次都不应长时间阻塞
        UiWatchdog wd(300, 50);
        wd.start();
        for (int i = 0; i < 5; ++i) {
            QCoreApplication::processEvents();
            w.refreshCurveDisplay();
            QCoreApplication::processEvents();
        }
        QCoreApplication::processEvents();
        wd.stop();

        QVERIFY2(wd.violationCount() == 0,
                 qPrintable(QString("filter-switch violations: %1").arg(wd.firstViolation())));
    }

    // ---- WF-09 P2：clearAll 重填下拉不得重入崩溃 ----
    void test_clearAllNoReentrantCrash()
    {
        HistoryReplayWidget w;
        w.resize(800, 400);
        w.show();
        QVERIFY(QTest::qWaitForWindowExposed(&w));

        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        makeCsv(tmp.filePath("a.csv"), 1000, 4);
        w.loadCSV(tmp.filePath("a.csv"));

        for (int i = 0; i < 50; ++i) {
            w.clearAll();  // 不应因重入 refreshCurveDisplay 崩溃/死循环
        }
        QCoreApplication::processEvents();
        QVERIFY(true);
    }

    // ---- SYM：打开通道过滤下拉 → 输入选择 → 立即拖动曲线区，看门狗零违规 ----
    void test_filterComboThenImmediateDrag()
    {
        HistoryReplayWidget w;
        w.resize(800, 400);
        w.show();
        QVERIFY(QTest::qWaitForWindowExposed(&w));

        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        const QString symCsv = tmp.filePath("sym.csv");
        makeCsv(symCsv, 20000, 6);
        w.loadCSV(symCsv);

        UiWatchdog wd(300, 50);
        wd.start();

        // 在下拉中选择通道（触发 refreshCurveDisplay）
        auto* combo = w.findChild<QComboBox*>();
        QVERIFY(combo);
        if (combo->count() > 1) {
            combo->setCurrentIndex(1);
        }
        QCoreApplication::processEvents();

        // 立即在曲线区做一次右键平移拖拽
        CurveWidget* cw = w.curveWidget();
        QVERIFY(cw);
        QTest::mousePress(cw, Qt::RightButton, {}, QPoint(400, 200));
        for (int i = 0; i < 40; ++i) {
            QTest::mouseMove(cw, QPoint(400 - i, 200));
        }
        QTest::mouseRelease(cw, Qt::RightButton, {}, QPoint(360, 200));
        QCoreApplication::processEvents();
        wd.stop();

        QVERIFY2(wd.violationCount() == 0,
                 qPrintable(QString("SYM drag violations: %1").arg(wd.firstViolation())));
    }
};

QTEST_MAIN(TestHistoryReplayInteraction)
#include "test_historyreplay_interaction.moc"
