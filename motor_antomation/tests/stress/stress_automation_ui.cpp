// ============================================================
// stress_automation_ui — 自动化域 UI 仿真压力测试（Phase 4）
// 固定种子随机交互序列 ≥500 次 + 事件循环看门狗（阈值 300ms）。
// 操作池：键入/Enter/Tab/双击添加/选节点/编辑参数/删除参数行/
//        拖动节点/拖拽连线/滚轮缩放/运行停止/切换视图/窗口 resize。
// 用法：stress_automation_ui --seed <N>（ctest 注册 3 个种子）
// 验收：3 种子全绿、看门狗零触发、控件树状态自检通过。
// ============================================================

#include <QtTest/QtTest>
#include <QLineEdit>
#include <QTableWidget>
#include <QPushButton>
#include <QComboBox>
#include <QTreeWidget>
#include <QTextEdit>
#include <QWheelEvent>
#include <random>
#include <chrono>

#include "ui/AutomationWidget.h"
#include "ui/FlowCanvas.h"
#include "ui/NodeLibraryPanel.h"
#include "ui/NodeParamPanel.h"
#include "ui/VariableEditorPanel.h"
#include "automation/AutomationEngine.h"
#include "common/watchdog.h"

using namespace MotorStudio;

namespace {
const char* kNodeTypes[] = {
    "SetParameter", "StartMotor", "Delay", "Wait", "If", "While",
    "For", "SendCommand", "ReadParameter", "Assert", "LogOutput", "Comment"
};
const int kNodeTypeCount = static_cast<int>(sizeof(kNodeTypes) / sizeof(kNodeTypes[0]));
}

class StressAutomationUi : public QObject {
    Q_OBJECT
public:
    int seed = 1;

private:
    UiWatchdog m_wd;
    AutomationEngine m_engine;
    AutomationWidget* m_w = nullptr;
    QStringList m_ops;            // 操作日志（QFAIL 时输出上下文）
    int m_opCount = 0;
    int m_realOps = 0;            // 真正执行了实际交互的次数（防空转）

    std::mt19937 m_rng;
    int randInt(int lo, int hi)   { return static_cast<int>(m_rng() % (hi - lo + 1)) + lo; }
    bool chance(int pct)          { return static_cast<int>(m_rng() % 100) < pct; }
    QString randText()
    {
        const char* words[] = {"speed","temp","ia","ib","1000","abc","x1","loop","a b c","12345"};
        const int n = static_cast<int>(sizeof(words)/sizeof(words[0]));
        return QString::fromLatin1(words[m_rng() % n]);
    }

    void record(const QString& op) { ++m_realOps; m_ops.append(QString::number(m_opCount) + ":" + op); }

    FlowCanvas* canvas() const   { return m_w->findChild<FlowCanvas*>(); }
    NodeParamPanel* panel() const{ return m_w->findChild<NodeParamPanel*>(); }
    NodeLibraryPanel* library() const { return m_w->findChild<NodeLibraryPanel*>(); }

    QList<FlowNodeItem*> nodes() const
    {
        QList<FlowNodeItem*> r;
        for (auto* it : canvas()->scene()->items())
            if (auto* ni = dynamic_cast<FlowNodeItem*>(it)) r.append(ni);
        return r;
    }

    QLineEdit* randomParamEdit()
    {
        auto edits = panel()->findChildren<QLineEdit*>();
        if (edits.isEmpty()) return nullptr;
        return edits[m_rng() % edits.size()];
    }

    QTableWidget* paramTable() const
    {
        for (auto* t : panel()->findChildren<QTableWidget*>())
            if (t->columnCount() == 3) return t;
        return nullptr;
    }

    QPushButton* findButton(const QString& text) const
    {
        for (auto* b : m_w->findChildren<QPushButton*>())
            if (b->text() == text) return b;
        return nullptr;
    }

    // ---------------- 操作原语 ----------------
    void opTypeText()
    {
        if (auto* le = randomParamEdit()) {
            le->setFocus();
            QTest::keyClicks(le, randText());
            record("type:" + le->property("paramKey").toString());
        }
    }

    void opPressEnter()
    {
        if (auto* le = randomParamEdit()) QTest::keyClick(le, Qt::Key_Return);
        else if (auto* t = paramTable()) { t->setFocus(); QTest::keyClick(t, Qt::Key_Return); }
        record("enter");
    }

    void opPressTab()
    {
        if (auto* le = randomParamEdit()) QTest::keyClick(le, Qt::Key_Tab);
        record("tab");
    }

    void opDoubleClickAdd()
    {
        auto* tree = library()->findChild<QTreeWidget*>();
        if (!tree || tree->topLevelItemCount() == 0) return;
        int cat = randInt(0, tree->topLevelItemCount() - 1);
        auto* catItem = tree->topLevelItem(cat);
        if (!catItem || catItem->childCount() == 0) return;
        QTreeWidgetItem* leaf = catItem->child(m_rng() % catItem->childCount());
        tree->setFocus();
        tree->scrollToItem(leaf);
        QRect r = tree->visualItemRect(leaf);
        if (!r.isValid()) return;
        QTest::mouseDClick(tree->viewport(), Qt::LeftButton, Qt::NoModifier, r.center());
        record("dblclick-add:" + leaf->text(0));
    }

    void opSelectNode()
    {
        auto ns = nodes();
        if (ns.isEmpty()) return;
        auto* ni = ns[m_rng() % ns.size()];
        canvas()->scene()->clearSelection();
        ni->setSelected(true);
        QTest::qWait(1);
        record("select:" + QString::fromStdString(ni->node().id));
    }

    void opEditParamCell()
    {
        auto* t = paramTable();
        if (!t || t->rowCount() == 0) return;
        int row = randInt(0, t->rowCount() - 1);
        int col = chance(70) ? 1 : 0;    // 70% 值列，30% 键列
        auto* item = t->item(row, col);
        if (!item) return;
        item->setText(randText());
        record("edit-cell:" + QString::number(row) + "," + QString::number(col));
    }

    void opDeleteParamRow()
    {
        auto* t = paramTable();
        if (!t || t->rowCount() == 0) return;
        int row = randInt(0, t->rowCount() - 1);
        auto* delBtn = qobject_cast<QPushButton*>(t->cellWidget(row, 2));
        if (!delBtn) return;
        QTest::mouseClick(delBtn, Qt::LeftButton);
        record("del-param-row:" + QString::number(row));
    }

    void opDragNode()
    {
        auto ns = nodes();
        if (ns.isEmpty()) return;
        auto* ni = ns[m_rng() % ns.size()];
        QPointF sc = ni->scenePos() + QPointF(FlowNodeItem::NODE_WIDTH / 2.0,
                                             FlowNodeItem::NODE_HEIGHT / 2.0);
        QPoint vp = canvas()->mapFromScene(sc);
        QTest::mousePress(canvas()->viewport(), Qt::LeftButton, Qt::NoModifier, vp);
        for (int i = 0; i < 8; ++i)
            QTest::mouseMove(canvas()->viewport(),
                             vp + QPoint(randInt(-30, 30), randInt(-20, 20)));
        QTest::mouseRelease(canvas()->viewport(), Qt::LeftButton, Qt::NoModifier,
                            vp + QPoint(randInt(-30, 30), randInt(-20, 20)));
        record("drag-node:" + QString::fromStdString(ni->node().id));
    }

    void opDragEdge()
    {
        auto ns = nodes();
        if (ns.size() < 2) return;
        auto* from = ns[m_rng() % ns.size()];
        auto* to   = ns[m_rng() % ns.size()];
        if (from == to) to = ns[(ns.indexOf(from) + 1) % ns.size()];
        QPoint fromVp = canvas()->mapFromScene(from->outputPortPos());
        QPoint toVp   = canvas()->mapFromScene(to->inputPortPos());
        QTest::mousePress(canvas()->viewport(), Qt::LeftButton, Qt::NoModifier, fromVp);
        QTest::mouseMove(canvas()->viewport(), (fromVp + toVp) / 2);
        QTest::mouseRelease(canvas()->viewport(), Qt::LeftButton, Qt::NoModifier, toVp);
        record("drag-edge:" + QString::fromStdString(from->node().id) + "->"
               + QString::fromStdString(to->node().id));
    }

    void opWheelZoom()
    {
        QPoint vp = canvas()->mapFromScene(QPointF(0, 0));
        QWheelEvent ev(vp, canvas()->viewport()->mapToGlobal(vp), QPoint(0, 0),
                       QPoint(0, chance(50) ? 120 : -120), Qt::NoButton, Qt::ControlModifier,
                       Qt::NoScrollPhase, false);
        QApplication::sendEvent(canvas()->viewport(), &ev);
        record("wheel-zoom");
    }

    void opRunStop()
    {
        auto* runBtn = findButton(QStringLiteral("运行"));
        auto* stopBtn = findButton(QStringLiteral("停止"));
        if (runBtn && runBtn->isEnabled()) {
            QTest::mouseClick(runBtn, Qt::LeftButton);
            record("run");
            QTest::qWait(20);
            if (stopBtn) {
                QTest::mouseClick(stopBtn, Qt::LeftButton);
                record("stop");
            }
            QTest::qWait(30);
        }
    }

    void opToggleView()
    {
        auto* toggle = findButton(QStringLiteral("表格视图"));
        if (!toggle) toggle = findButton(QStringLiteral("流程图视图"));
        if (toggle) { QTest::mouseClick(toggle, Qt::LeftButton); record("toggle-view"); }
    }

    void opResize()
    {
        m_w->resize(randInt(700, 1200), randInt(500, 800));
        QTest::qWait(1);
        record("resize");
    }

    void opTextEdit()
    {
        // 若节点含 QTextEdit（If/Assert 条件），输入随机文本
        auto teds = panel()->findChildren<QTextEdit*>();
        if (!teds.isEmpty()) {
            auto* te = teds[m_rng() % teds.size()];
            te->clear();
            QTest::keyClicks(te, randText());
            record("textedit");
        }
    }

    void opDeleteNode()
    {
        auto ns = nodes();
        if (ns.isEmpty()) return;
        auto* ni = ns[m_rng() % ns.size()];
        canvas()->scene()->clearSelection();
        ni->setSelected(true);
        QTest::keyClick(canvas(), Qt::Key_Delete);
        record("delete-node:" + QString::fromStdString(ni->node().id));
    }

    void runOp(int op)
    {
        switch (op) {
        case 0: opTypeText(); break;
        case 1: opPressEnter(); break;
        case 2: opPressTab(); break;
        case 3: opDoubleClickAdd(); break;
        case 4: opSelectNode(); break;
        case 5: opEditParamCell(); break;
        case 6: opDeleteParamRow(); break;
        case 7: opDragNode(); break;
        case 8: opDragEdge(); break;
        case 9: opWheelZoom(); break;
        case 10: opRunStop(); break;
        case 11: opToggleView(); break;
        case 12: opResize(); break;
        case 13: opTextEdit(); break;
        case 14: opDeleteNode(); break;
        default: break;
        }
        // 0~5ms 随机延迟
        if (m_rng() % 100 < 80) QTest::qWait(m_rng() % 6);
    }

private slots:
    void initTestCase()
    {
        // 预置示例流程图（≥10 节点，含 If/Assert/参数节点）
        m_w = new AutomationWidget(&m_engine);
        m_w->resize(1000, 700);
        m_w->show();
        QTest::qWait(30);

        auto* canvas = this->canvas();
        QVERIFY(canvas);
        for (int i = 0; i < 12; ++i)
            canvas->addNodeFromPalette(kNodeTypes[i % kNodeTypeCount]);
        // 连几条边
        auto ns = nodes();
        for (int i = 0; i + 1 < ns.size(); i += 2) {
            QPoint f = canvas->mapFromScene(ns[i]->outputPortPos());
            QPoint t = canvas->mapFromScene(ns[i + 1]->inputPortPos());
            QTest::mousePress(canvas->viewport(), Qt::LeftButton, Qt::NoModifier, f);
            QTest::mouseMove(canvas->viewport(), (f + t) / 2);
            QTest::mouseRelease(canvas->viewport(), Qt::LeftButton, Qt::NoModifier, t);
        }
        QTest::qWait(20);
    }

    void init()
    {
        m_rng.seed(static_cast<unsigned>(seed));
        m_ops.clear();
        m_opCount = 0;
        m_wd.start(m_w);
    }

    void stressRun()
    {
        const int kOps = 500;
        for (m_opCount = 0; m_opCount < kOps; ++m_opCount) {
            runOp(m_rng() % 15);
            // 每 100 次检查一次看门狗，卡死尽早暴露
            if (m_opCount % 100 == 99) {
                QVERIFY2(m_wd.violationCount() == 0,
                    qPrintable(QStringLiteral("seed=%1 卡死@op%2: %3")
                        .arg(seed).arg(m_opCount)
                        .arg(m_wd.violations().isEmpty() ? QString()
                             : m_wd.violations().first() + " | lastOps: " + m_ops.mid(qMax(0, m_ops.size()-5)).join(" ; "))));
                if (m_wd.violationCount() > 0) break;
            }
        }

        // 收尾：看门狗零违规
        QTest::qWait(50);
        m_wd.stop();
        QVERIFY2(m_wd.violationCount() == 0,
            qPrintable(QStringLiteral("seed=%1 结尾看门狗违规: %2")
                .arg(seed).arg(m_wd.violations().isEmpty() ? QString()
                     : m_wd.violations().first() + " | lastOps: " + m_ops.mid(qMax(0, m_ops.size()-5)).join(" ; "))));

        // 防空转：≥500 次操作池抽取中，至少 25% 真正执行了交互
        QVERIFY2(m_realOps >= kOps / 4,
            qPrintable(QStringLiteral("seed=%1 真实操作过少: %2/%3")
                .arg(seed).arg(m_realOps).arg(kOps)));
        // 状态自检：canvas 节点数 == 数据模型节点数（无泄漏式增长/脱钩）
        int sceneNodes = 0;
        for (auto* it : canvas()->scene()->items())
            if (dynamic_cast<FlowNodeItem*>(it)) ++sceneNodes;
        int modelNodes = static_cast<int>(canvas()->flowGraph()->nodes.size());
        QVERIFY2(sceneNodes == modelNodes,
            qPrintable(QStringLiteral("seed=%1 场景/模型节点数不一致: scene=%2 model=%3")
                .arg(seed).arg(sceneNodes).arg(modelNodes)));
        // 参数表行数不得超过节点可见参数数（无行泄漏）
        if (auto* t = paramTable()) {
            int visible = 0;
            if (canvas()->flowGraph() && !canvas()->flowGraph()->nodes.empty()) {
                for (const auto& n : canvas()->flowGraph()->nodes)
                    for (const auto& p : n.params)
                        if (!p.first.empty() && p.first[0] != '_') ++visible;
            }
            QVERIFY2(t->rowCount() <= qMax(1, visible),
                qPrintable(QStringLiteral("seed=%1 参数表行数异常: %2 visible=%3")
                    .arg(seed).arg(t->rowCount()).arg(visible)));
        }
    }

    void cleanup()
    {
        m_wd.stop();
    }

    void cleanupTestCase()
    {
        delete m_w;
        m_w = nullptr;
    }
};

// 自定义 main：解析 --seed <N>，其余参数交给 QTest
int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    int seed = 1;
    QStringList filtered;
    const QStringList args = app.arguments();
    for (int i = 1; i < args.size(); ++i) {
        if (args[i] == QLatin1String("--seed") && i + 1 < args.size()) {
            seed = args[i + 1].toInt();
            ++i;
            continue;
        }
        filtered << args[i];
    }
    StressAutomationUi tc;
    tc.seed = seed;
    return QTest::qExec(&tc, filtered);
}
#include "stress_automation_ui.moc"
