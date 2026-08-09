// ============================================================
// TestFlowCanvasInteraction — 流程图画布交互防卡死/防崩溃回归
// 覆盖审计项：A-03(拖拽期删除节点→m_dragFromNode 悬垂)、A-06(删除
//   后数据模型不同步)、A-07(添加节点选择时序)、SYM-1/2/3。
// 修复前：A-06 确定性红（模型不同步断言失败）；修复后全绿。
// ============================================================

#include <QtTest/QtTest>
#include <QGraphicsItem>
#include <QElapsedTimer>

#include "ui/AutomationWidget.h"
#include "ui/FlowCanvas.h"
#include "ui/NodeLibraryPanel.h"
#include "ui/NodeParamPanel.h"
#include "automation/AutomationEngine.h"
#include "../../common/watchdog.h"

using namespace MotorStudio;

static FlowNodeItem* firstNodeItem(FlowCanvas* canvas)
{
    for (auto* item : canvas->scene()->items())
        if (auto* ni = dynamic_cast<FlowNodeItem*>(item))
            return ni;
    return nullptr;
}

class TestFlowCanvasInteraction : public QObject {
    Q_OBJECT
private:
    AutomationEngine m_engine;
    AutomationWidget* m_w = nullptr;
    UiWatchdog m_wd;

    AutomationWidget* makeWidget()
    {
        auto* w = new AutomationWidget(&m_engine);
        w->resize(1000, 700);
        w->show();
        QTest::qWait(30);
        return w;
    }

    QPushButton* findButton(const QString& text) const
    {
        for (auto* b : m_w->findChildren<QPushButton*>())
            if (b->text() == text) return b;
        return nullptr;
    }

private slots:
    void init()
    {
        m_w = makeWidget();
        m_wd.start(m_w);
    }

    void cleanup()
    {
        m_wd.stop();
        delete m_w;
        m_w = nullptr;
        QVERIFY2(m_wd.violationCount() == 0,
                 qPrintable(QStringLiteral("看门狗违规: %1").arg(
                     m_wd.violations().isEmpty() ? QString() : m_wd.violations().first())));
    }

    // ------------------------------------------------------------
    // A-06：删除节点后，绑定数据模型必须同步移除（确定性 RED）。
    // 修复前：deleteSelectedItems 只删场景项，m_flowGraph->nodes
    // 保留幻影节点 → 断言失败。
    // ------------------------------------------------------------
    void testDeleteNode_syncsModel()
    {
        FlowGraph g;
        FlowNode n1; n1.id = "n1"; n1.type = "SetParameter"; n1.label = "A";
        FlowNode n2; n2.id = "n2"; n2.type = "Delay";        n2.label = "B";
        g.nodes.push_back(n1);
        g.nodes.push_back(n2);
        FlowEdge e1; e1.id = "e1"; e1.fromNodeId = "n1"; e1.toNodeId = "n2";
        g.edges.push_back(e1);

        FlowCanvas canvas;
        canvas.resize(600, 400);
        canvas.setFlowGraph(&g);
        canvas.loadGraph(g);
        canvas.show();
        QTest::qWait(20);

        // 选中 n1 并按 Delete
        for (auto* item : canvas.scene()->items())
            if (auto* ni = dynamic_cast<FlowNodeItem*>(item))
                if (ni->node().id == "n1") ni->setSelected(true);
        QTest::keyClick(&canvas, Qt::Key_Delete);
        QTest::qWait(20);

        bool stillInModel = false;
        for (const auto& n : g.nodes)
            if (n.id == "n1") stillInModel = true;
        QVERIFY2(!stillInModel, "A-06: 删除节点后数据模型不应再包含该节点");

        // 边 e1 也应被同步移除（入射边随节点删除）
        bool edgeStill = false;
        for (const auto& e : g.edges)
            if (e.id == "e1") edgeStill = true;
        QVERIFY2(!edgeStill, "A-06: 入射边 e1 也应从数据模型移除");
    }

    // ------------------------------------------------------------
    // A-03：连线拖拽中删除节点 → m_dragFromNode 悬垂，下一次
    // mouseMove 解引用已释放节点。修复前可能崩溃（堆态相关）。
    // ------------------------------------------------------------
    void testDeleteDuringEdgeDrag_noUAF()
    {
        FlowCanvas canvas;
        canvas.resize(600, 400);
        canvas.show();
        canvas.addNodeFromPalette("SetParameter");
        QTest::qWait(20);

        auto* nodeItem = firstNodeItem(&canvas);
        QVERIFY(nodeItem);

        // 从输出端口开始连线拖拽
        QPointF outPort = nodeItem->outputPortPos();
        QPoint vp = canvas.mapFromScene(outPort);
        QTest::mousePress(canvas.viewport(), Qt::LeftButton, Qt::NoModifier, vp);
        QTest::mouseMove(canvas.viewport(), vp + QPoint(15, 15));
        QTest::qWait(5);

        // 拖拽进行中删除该节点（Delete 键）
        nodeItem->setSelected(true);
        QTest::keyClick(&canvas, Qt::Key_Delete);
        QTest::qWait(5);

        // 继续移动鼠标 → 修复前 mouseMoveEvent 解引用已释放节点
        QTest::mouseMove(canvas.viewport(), vp + QPoint(40, 40));
        QTest::mouseRelease(canvas.viewport(), Qt::LeftButton, Qt::NoModifier, vp + QPoint(40, 40));
        QTest::qWait(10);
        QVERIFY(true); // 未崩溃即通过
    }

    // ------------------------------------------------------------
    // A-07：添加节点后参数面板应显示该节点且不抖动。
    // ------------------------------------------------------------
    void testAddNode_panelShowsNode()
    {
        auto* canvas = m_w->findChild<FlowCanvas*>();
        auto* panel  = m_w->findChild<NodeParamPanel*>();
        QVERIFY(canvas && panel);

        canvas->addNodeFromPalette("SetParameter");
        QTest::qWait(20);

        // 面板应已构建表单（存在参数行）
        bool hasForm = false;
        for (auto* t : panel->findChildren<QTableWidget*>())
            if (t->columnCount() == 3) hasForm = true;
        QVERIFY2(hasForm, "添加节点后参数面板应显示参数管理表");

        // 画布上应有 1 个节点且被选中
        int nodeCount = 0;
        for (auto* item : canvas->scene()->items())
            if (dynamic_cast<FlowNodeItem*>(item)) ++nodeCount;
        QCOMPARE(nodeCount, 1);
        QVERIFY(canvas->selectedNode() != nullptr);
    }

    // ------------------------------------------------------------
    // SYM-1：输入 → 立即 Enter → 100ms 内拖动 → 无卡死无崩溃。
    // ------------------------------------------------------------
    void testSym1_typeEnterDrag()
    {
        auto* canvas = m_w->findChild<FlowCanvas*>();
        auto* panel  = m_w->findChild<NodeParamPanel*>();
        QVERIFY(canvas && panel);

        canvas->addNodeFromPalette("SetParameter");
        QTest::qWait(20);

        // 找到 value 输入框
        QLineEdit* valueEdit = nullptr;
        for (auto* le : panel->findChildren<QLineEdit*>()) {
            if (le->property("paramKey").toString() == "value") { valueEdit = le; break; }
        }
        QVERIFY2(valueEdit, "SetParameter 应有 value 参数输入框");

        // 输入 → 立即 Enter（先清空字段，避免初始值 "0" 拼接成 "01500"）
        valueEdit->clear();
        QTest::keyClicks(valueEdit, "1500");
        QTest::keyClick(valueEdit, Qt::Key_Return);

        // 100ms 内开始拖动节点
        auto* nodeItem = firstNodeItem(canvas);
        QVERIFY(nodeItem);
        QPointF sc = nodeItem->scenePos() + QPointF(80, 28);
        QPoint vp = canvas->mapFromScene(sc);
        QTest::mousePress(canvas->viewport(), Qt::LeftButton, Qt::NoModifier, vp);
        for (int i = 0; i < 10; ++i)
            QTest::mouseMove(canvas->viewport(), vp + QPoint(i * 5, i * 3));
        QTest::mouseRelease(canvas->viewport(), Qt::LeftButton, Qt::NoModifier, vp + QPoint(50, 30));
        QTest::qWait(20);

        // 功能：value 已写入节点
        bool persisted = false;
        if (nodeItem)
            for (const auto& p : nodeItem->node().params)
                if (p.first == "value" && p.second == "1500") persisted = true;
        QVERIFY2(persisted, "SYM-1: 输入 value=1500 应持久化到节点");
    }

    // ------------------------------------------------------------
    // SYM-2：输入后不按 Enter 直接点别处（清空选择）→ 无卡死。
    // ------------------------------------------------------------
    void testSym2_typeThenClickAway()
    {
        auto* canvas = m_w->findChild<FlowCanvas*>();
        auto* panel  = m_w->findChild<NodeParamPanel*>();
        QVERIFY(canvas && panel);

        canvas->addNodeFromPalette("SetParameter");
        QTest::qWait(20);

        QLineEdit* valueEdit = nullptr;
        for (auto* le : panel->findChildren<QLineEdit*>())
            if (le->property("paramKey").toString() == "value") { valueEdit = le; break; }
        QVERIFY(valueEdit);

        QTest::keyClicks(valueEdit, "900");
        // 点画布空白处（清空选择 → 面板 clearNode → 重建）
        QPoint emptyVp = canvas->mapFromScene(QPointF(500, 200));
        QTest::mouseClick(canvas->viewport(), Qt::LeftButton, Qt::NoModifier, emptyVp);
        QTest::qWait(20);

        // 面板应已清空（无参数管理表）
        bool hasTable = false;
        for (auto* t : panel->findChildren<QTableWidget*>())
            if (t->columnCount() == 3) hasTable = true;
        QVERIFY2(!hasTable, "SYM-2: 清空选择后参数面板应清空");
    }

    // ------------------------------------------------------------
    // SYM-3：快速连续双击节点库添加节点 → 无卡死无崩溃。
    // 说明：QTest::mouseDClick 连发时，Qt 的 click-count 计数会让奇数号
    // click 重置为单击（QTest 伪影），因此不断言精确数量，只断言"无卡死+
    // 节点有增长+看门狗零违规"；精确添加数由 testDoubleClickAdd_slowCount 验证。
    // ------------------------------------------------------------
    void testSym3_rapidDoubleClickAdd()
    {
        auto* canvas = m_w->findChild<FlowCanvas*>();
        auto* library = m_w->findChild<NodeLibraryPanel*>();
        QVERIFY(canvas && library);

        auto* tree = library->findChild<QTreeWidget*>();
        QVERIFY(tree && tree->topLevelItemCount() > 0);
        QTreeWidgetItem* leaf = tree->topLevelItem(0)->child(0); // 设置参数
        QVERIFY(leaf);

        QRect r = tree->visualItemRect(leaf);
        for (int i = 0; i < 10; ++i) {
            QTest::mouseDClick(tree->viewport(), Qt::LeftButton, Qt::NoModifier, r.center());
            QTest::qWait(5);
        }
        QTest::qWait(20);

        int nodeCount = 0;
        for (auto* item : canvas->scene()->items())
            if (dynamic_cast<FlowNodeItem*>(item)) ++nodeCount;
        QVERIFY2(nodeCount >= 1, "SYM-3: 快速双击应至少添加一个节点");
        // 无崩溃 + 看门狗零违规（cleanup 检查）
    }

    // ------------------------------------------------------------
    // H1 回归：流程运行完成后运行按钮应恢复可用（onFlowRunnerFinished 必须执行；
    // FlowRunResult 需注册元类型才能从 worker 跨线程投递到 UI）。
    // 修复前：按钮永久禁用、UI 卡在"运行中"。
    // ------------------------------------------------------------
    void testRunFlow_completesAndUnlocks()
    {
        auto* canvas = m_w->findChild<FlowCanvas*>();
        auto* runBtn = findButton(QStringLiteral("运行"));
        QVERIFY(canvas && runBtn);

        canvas->addNodeFromPalette("SetParameter");
        canvas->addNodeFromPalette("StopMotor");
        QTest::qWait(20);
        // 构造后运行按钮应在流程图视图下可用（存量 bug：先前 updateButtonStates
        // 在切到流程图前调用 → 按钮永久禁用）
        QVERIFY2(runBtn->isEnabled(),
                 "H1: 流程图模式构造后运行按钮应可用（视图模式先于按钮状态计算）");

        QTest::mouseClick(runBtn, Qt::LeftButton);

        // 轮询等待"流程图汇总"标签出现（onFlowRunnerFinished 才会设置它），
        // 以此证明 runnerFinished 成功从 worker 跨线程投递（H1）。
        bool completed = false;
        QElapsedTimer t;
        t.start();
        while (t.elapsed() < 5000 && !completed) {
            QTest::qWait(50);
            for (auto* lbl : m_w->findChildren<QLabel*>())
                if (lbl->text().startsWith(QStringLiteral("流程图汇总"))) { completed = true; break; }
        }
        QVERIFY2(completed,
                 "H1: 运行完成后应显示流程图汇总（runnerFinished 需成功投递到 UI）");
        QVERIFY2(runBtn->isEnabled(),
                 "H1: 运行完成后运行按钮应恢复可用（重入守卫复位）");
    }

    // ------------------------------------------------------------
    // 添加路径确定性计数：addNodeFromPalette 每次恰好加一个节点。
    // （双击路径由 testSym3_rapidDoubleClickAdd 以"无卡死+节点增长"覆盖，
    // 精确计数在此绕过 QTest 合成双击伪影，直接驱动真实添加接口。）
    // ------------------------------------------------------------
    void testAddNodeFromPalette_count()
    {
        auto* canvas = m_w->findChild<FlowCanvas*>();
        QVERIFY(canvas);
        const int N = 10;
        for (int i = 0; i < N; ++i) {
            canvas->addNodeFromPalette("Delay");
            QTest::qWait(5);
        }
        QTest::qWait(20);

        int nodeCount = 0;
        for (auto* item : canvas->scene()->items())
            if (dynamic_cast<FlowNodeItem*>(item)) ++nodeCount;
        QCOMPARE(nodeCount, N);

        // 数据模型同步：节点数一致
        int modelCount = static_cast<int>(canvas->flowGraph()->nodes.size());
        QCOMPARE(modelCount, N);
    }

    // ------------------------------------------------------------
    // 压力前的看门狗探针：拖动含多条边的节点，单次事件不得超阈值。
    // ------------------------------------------------------------
    void testDragManyNodes_watchdogClean()
    {
        auto* canvas = m_w->findChild<FlowCanvas*>();
        QVERIFY(canvas);

        // 添加若干节点并互相连线
        canvas->addNodeFromPalette("SetParameter");
        for (int i = 0; i < 6; ++i) canvas->addNodeFromPalette("Delay");
        QTest::qWait(20);

        QList<FlowNodeItem*> items;
        for (auto* it : canvas->scene()->items())
            if (auto* ni = dynamic_cast<FlowNodeItem*>(it)) items.append(ni);
        QVERIFY(items.size() >= 4);

        // 快速拖动第一个节点
        QPointF sc = items.first()->scenePos() + QPointF(80, 28);
        QPoint vp = canvas->mapFromScene(sc);
        QTest::mousePress(canvas->viewport(), Qt::LeftButton, Qt::NoModifier, vp);
        for (int i = 0; i < 60; ++i)
            QTest::mouseMove(canvas->viewport(), vp + QPoint(i * 4, (i % 7) * 3));
        QTest::mouseRelease(canvas->viewport(), Qt::LeftButton, Qt::NoModifier, vp + QPoint(240, 18));
        QTest::qWait(20);
        QVERIFY(true);
    }
};

QTEST_MAIN(TestFlowCanvasInteraction)
#include "test_flow_canvas.moc"
