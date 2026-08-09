// ============================================================
// TestVariableEditorPanel — 变量表编辑 + 作用域接线回归
// 覆盖审计项：A-14（setScope 从未被调用 → 面板失效）。
// 修复前：AutomationWidget 中的变量表面板 scope() 为 null（RED）；
//   修复后：绑定 VariableScope，编辑生效。
// ============================================================

#include <QtTest/QtTest>
#include <QTableWidget>
#include <QComboBox>
#include <QLineEdit>
#include <QPushButton>
#include <QDialog>
#include <QTimer>
#include <QApplication>

#include "ui/AutomationWidget.h"
#include "ui/VariableEditorPanel.h"
#include "automation/AutomationEngine.h"
#include "automation/VariableScope.h"

using namespace MotorStudio;

class TestVariableEditorPanel : public QObject {
    Q_OBJECT
private slots:
    // ------------------------------------------------------------
    // A-14：AutomationWidget 构造后，其变量表面板必须已绑定作用域。
    // 修复前：AutomationWidget.cpp:352 只 new VariableEditorPanel，
    //   从未 setScope → scope()==nullptr → 面板静默失效（RED）。
    // ------------------------------------------------------------
    void testAutomationWidget_varPanelHasScope()
    {
        AutomationEngine engine;
        AutomationWidget w(&engine);
        w.resize(1000, 700);
        w.show();
        QTest::qWait(20);

        auto* varPanel = w.findChild<VariableEditorPanel*>();
        QVERIFY2(varPanel, "AutomationWidget 应包含变量表面板");
        QVERIFY2(varPanel->scope() != nullptr,
                 "A-14: 变量表面板应绑定作用域（修复前未调用 setScope，此处红）");
    }

    // ------------------------------------------------------------
    // 基本功能：绑定作用域后，添加/编辑变量应生效。
    // ------------------------------------------------------------
    void testAddAndEditVariable()
    {
        VariableScope scope;
        VariableEditorPanel panel;
        panel.setScope(&scope);
        panel.resize(300, 200);
        panel.show();
        QTest::qWait(10);

        QPushButton* addBtn = nullptr;
        for (auto* b : panel.findChildren<QPushButton*>())
            if (b->text() == QStringLiteral("+ 添加变量")) { addBtn = b; break; }
        QVERIFY(addBtn);

        // 自动应答"添加变量"对话框
        bool answered = false;
        QTimer autoAnswer;
        autoAnswer.setInterval(30);
        QObject::connect(&autoAnswer, &QTimer::timeout, [&]() {
            auto* dlg = qobject_cast<QDialog*>(QApplication::activeModalWidget());
            if (!dlg) return;
            if (auto* le = dlg->findChild<QLineEdit*>()) le->setText(QStringLiteral("myVar"));
            dlg->accept();
            answered = true;
        });
        autoAnswer.start();
        QTest::mouseClick(addBtn, Qt::LeftButton);
        autoAnswer.stop();
        QTest::qWait(10);

        QVERIFY2(answered, "添加变量对话框应被应答");
        QVERIFY(scope.has("myVar"));

        // 编辑值单元格：Number 类型，值 -> 3.5
        auto* table = panel.findChild<QTableWidget*>();
        QVERIFY(table && table->rowCount() >= 1);
        int row = -1;
        for (int i = 0; i < table->rowCount(); ++i) {
            auto* it = table->item(i, 0);
            if (it && it->text() == QStringLiteral("myVar")) { row = i; break; }
        }
        QVERIFY(row >= 0);
        auto* valItem = table->item(row, 2);
        QVERIFY(valItem);
        valItem->setText(QStringLiteral("3.5"));
        QTest::qWait(10);

        auto v = scope.getNumber("myVar");
        QVERIFY(v.has_value());
        QCOMPARE(v.value(), 3.5);
    }

    // ------------------------------------------------------------
    // 防御：编辑单元格不触发变量表崩溃/卡死（看门狗）。
    // ------------------------------------------------------------
    void testEditCells_noStall()
    {
        VariableScope scope;
        for (int i = 0; i < 5; ++i)
            scope.setNumber("v" + std::to_string(i), i);
        VariableEditorPanel panel;
        panel.setScope(&scope);
        panel.resize(300, 200);
        panel.show();
        QTest::qWait(10);

        auto* table = panel.findChild<QTableWidget*>();
        QVERIFY(table && table->rowCount() == 5);
        for (int i = 0; i < table->rowCount(); ++i) {
            auto* it = table->item(i, 2);
            if (it) { it->setText(QStringLiteral("9")); QTest::qWait(5); }
        }
        QTest::qWait(10);
        QVERIFY(true);
    }
};

QTEST_MAIN(TestVariableEditorPanel)
#include "test_variable_editor_panel.moc"
