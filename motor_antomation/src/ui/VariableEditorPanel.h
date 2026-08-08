#pragma once

#include <QWidget>
#include <QTableWidget>
#include <QPushButton>
#include <QComboBox>
#include <QVBoxLayout>
#include <QLabel>

namespace MotorStudio {

class VariableScope;

// ============================================================
// VariableEditorPanel — simple variable table editor
//   Columns: 名称 | 类型 | 值
//   Buttons: + 添加变量 / 删除
// ============================================================
class VariableEditorPanel : public QWidget {
    Q_OBJECT
public:
    explicit VariableEditorPanel(QWidget* parent = nullptr);

    void setScope(VariableScope* scope);
    VariableScope* scope() const { return m_scope; }
    void refreshTable();

signals:
    void variablesChanged();

private slots:
    void onAddVariable();
    void onDeleteVariable();
    void onCellChanged(int row, int col);

private:
    void setupUi();

    VariableScope* m_scope = nullptr;
    QTableWidget*  m_table = nullptr;
    QPushButton*   m_addBtn = nullptr;
    QPushButton*   m_delBtn = nullptr;
};

} // namespace MotorStudio
