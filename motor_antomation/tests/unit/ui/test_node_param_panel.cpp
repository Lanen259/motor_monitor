// ============================================================
// TestNodeParamPanelInteraction — 参数面板交互防卡死/防崩溃回归
// 覆盖审计项：A-01(删除参数行按钮 UAF)、A-02(添加参数按钮 UAF)、
//            A-15(键列编辑重复)、BUG-001/004 buildForm 生命周期闭环。
// 修复前：A-01/A-02 应红（UAF 崩溃或断言失败）；修复后全绿。
// ============================================================

#include <QtTest/QtTest>
#include <QPushButton>
#include <QTableWidget>
#include <QLineEdit>
#include <QTimer>
#include <QInputDialog>

#include "ui/NodeParamPanel.h"
#include "automation/FlowGraph.h"

using namespace MotorStudio;

static FlowNode makeNode(const char* type, int nParams)
{
    FlowNode n;
    n.id = "n1";
    n.type = type;
    if (nParams >= 1) n.params.emplace_back("name", "Speed");
    if (nParams >= 2) n.params.emplace_back("value", "1000");
    if (nParams >= 3) n.params.emplace_back("extra", "x");
    return n;
}

class TestNodeParamPanel : public QObject {
    Q_OBJECT
private:
    NodeParamPanel* m_panel = nullptr;

    // 参数管理表：3 列（键/值/删除按钮）的那张 QTableWidget
    QTableWidget* paramTable() const
    {
        for (auto* t : m_panel->findChildren<QTableWidget*>())
            if (t->columnCount() == 3) return t;
        return nullptr;
    }

    QPushButton* findButtonByText(const QString& text) const
    {
        for (auto* b : m_panel->findChildren<QPushButton*>())
            if (b->text() == text) return b;
        return nullptr;
    }

private slots:
    void init()
    {
        m_panel = new NodeParamPanel();
        m_panel->resize(280, 600);
        m_panel->show();
        QTest::qWait(20);
    }

    void cleanup()
    {
        delete m_panel;
        m_panel = nullptr;
    }

    // ------------------------------------------------------------
    // A-01：点击参数管理表"×"删除按钮。
    // 触发 onDeleteParam → buildForm → clearForm 同步删除正在发
    // clicked() 的 delBtn（含整个 paramTable）→ use-after-free。
    // ------------------------------------------------------------
    void testDeleteParamRow_buttonUaf()
    {
        FlowNode n = makeNode("SetParameter", 2);
        m_panel->setNode(&n);
        QTest::qWait(10);

        QTableWidget* t = paramTable();
        QVERIFY2(t, "参数管理表应存在（3 列）");
        QVERIFY(t->rowCount() >= 1);

        auto* delBtn = qobject_cast<QPushButton*>(t->cellWidget(0, 2));
        QVERIFY2(delBtn, "第 0 行应有删除按钮");

        // 点击删除按钮 → 重建表单。修复前：clearForm 同步 delete 发送者 → UAF。
        QTest::mouseClick(delBtn, Qt::LeftButton);
        QTest::qWait(20);

        // 功能断言：删除后 param 数量减一，表单重建仍有效
        auto* t2 = paramTable();
        QVERIFY2(t2, "删除后表单应重建");
        QVERIFY2(t2->rowCount() <= 1, "删除一行后参数管理表应只剩 1 行");
        QCOMPARE(static_cast<int>(n.params.size()), 1);
    }

    // ------------------------------------------------------------
    // A-02：点击"+ 添加参数"按钮。
    // QInputDialog 嵌套事件循环悬挂 clicked() 发射帧，对话框返回后
    // buildForm→clearForm 删除正在发 clicked() 的 addParamBtn → UAF。
    // 测试用定时器自动应答两个输入对话框。
    // ------------------------------------------------------------
    void testAddParam_buttonUaf()
    {
        FlowNode n = makeNode("SetParameter", 1);
        m_panel->setNode(&n);
        QTest::qWait(10);

        auto* addBtn = findButtonByText(QStringLiteral("+ 添加参数"));
        QVERIFY2(addBtn, "应存在 + 添加参数 按钮");

        // 自动应答对话框：参数名/参数值
        int answered = 0;
        QTimer autoAnswer;
        autoAnswer.setInterval(30);
        QObject::connect(&autoAnswer, &QTimer::timeout, [&]() {
            auto* dlg = qobject_cast<QDialog*>(QApplication::activeModalWidget());
            if (!dlg) return;
            if (auto* le = dlg->findChild<QLineEdit*>()) {
                le->setText(answered == 0 ? QStringLiteral("newKey") : QStringLiteral("42"));
            }
            dlg->accept();
            ++answered;
        });
        autoAnswer.start();

        QTest::mouseClick(addBtn, Qt::LeftButton);
        autoAnswer.stop();
        QTest::qWait(20);

        // 功能断言：新增参数已写入节点
        bool hasNewKey = false;
        for (const auto& p : n.params)
            if (p.first == "newKey") hasNewKey = true;
        QVERIFY2(hasNewKey, "新增参数 newKey 应写入节点");
        QVERIFY2(answered >= 2, "两个输入对话框都应被应答");
    }

    // ------------------------------------------------------------
    // BUG-001/004 闭环：连续 setNode/clearNode 50 次，覆盖
    // If(QTextEdit)/Assert/SetParameter/未知类型(通用键值表)，不崩溃。
    // ------------------------------------------------------------
    void testSetNodeClearNode50x()
    {
        for (int i = 0; i < 50; ++i) {
            FlowNode n;
            n.id = "n" + std::to_string(i);
            switch (i % 4) {
            case 0: n.type = "SetParameter"; n.params = {{"name","Speed"},{"value","1000"}}; break;
            case 1: n.type = "If";           n.params = {{"expression","$温度 < 85"}}; break;
            case 2: n.type = "Assert";       n.params = {{"expression","channel:Ia>0"},{"message","fail"}}; break;
            default: n.type = "UnknownType"; n.params = {{"k1","v1"},{"k2","v2"}}; break;
            }
            m_panel->setNode(&n);
            m_panel->clearNode();
        }
        QTest::qWait(10);
        QVERIFY(true);
    }

    // ------------------------------------------------------------
    // A-15：参数管理表编辑"键"列不应产生重复条目。
    // 修复前：改键 → 找不到新键则 emplace_back，旧键残留 → 重复。
    // ------------------------------------------------------------
    void testParamKeyEditDedup()
    {
        FlowNode n = makeNode("SetParameter", 1);
        m_panel->setNode(&n);
        QTest::qWait(10);

        QTableWidget* t = paramTable();
        QVERIFY(t && t->rowCount() >= 1);

        // 编辑第 0 行键列：name -> renamed
        t->setItem(0, 0, new QTableWidgetItem(QStringLiteral("renamed")));
        QTest::qWait(10);

        bool hasRenamed = false;
        bool hasOldName = false;
        for (const auto& p : n.params) {
            if (p.first == "renamed") hasRenamed = true;
            if (p.first == "name")    hasOldName = true;
        }
        QVERIFY2(hasRenamed, "编辑后的键应存在");
        QVERIFY2(!hasOldName, "旧键不应残留（修复后无重复条目）");
    }

    // ------------------------------------------------------------
    // 防御：对带大量参数 + 未知类型的节点反复编辑，看门狗零超时。
    // ------------------------------------------------------------
    void testGenericTableEdit_noStall()
    {
        FlowNode n;
        n.id = "g1";
        n.type = "Mystery";
        for (int i = 0; i < 20; ++i)
            n.params.emplace_back("k" + std::to_string(i), "v" + std::to_string(i));
        m_panel->setNode(&n);
        QTest::qWait(10);

        // 找到通用键值表（2 列、有 _genericTable 属性）
        QTableWidget* generic = nullptr;
        for (auto* t : m_panel->findChildren<QTableWidget*>()) {
            if (t->property("_genericTable").toBool()) { generic = t; break; }
        }
        QVERIFY2(generic, "未知类型应渲染通用键值表");
        QVERIFY(generic->rowCount() >= 1);

        // 编辑若干值单元格
        for (int i = 0; i < 5; ++i) {
            QTableWidgetItem* it = generic->item(i, 1);
            if (it) { it->setText("updated" + QString::number(i)); QTest::qWait(5); }
        }
        QTest::qWait(10);
        QVERIFY(true);
    }
};

QTEST_MAIN(TestNodeParamPanel)
#include "test_node_param_panel.moc"
