#include "VariableEditorPanel.h"
#include "automation/VariableScope.h"

#include <QHeaderView>
#include <QInputDialog>
#include <QMessageBox>

namespace MotorStudio {

// ============================================================
// Constructor
// ============================================================

VariableEditorPanel::VariableEditorPanel(QWidget* parent)
    : QWidget(parent)
{
    setupUi();
}

void VariableEditorPanel::setupUi()
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(4);

    // --- Title ---
    auto* titleLabel = new QLabel(QStringLiteral("变量表"), this);
    titleLabel->setStyleSheet(R"(
        QLabel {
            font-family: 'Microsoft YaHei';
            font-size: 13px;
            font-weight: bold;
            color: #2196F3;
            padding: 6px 8px;
            border-bottom: 1px solid #E0E0E0;
        }
    )");
    layout->addWidget(titleLabel);

    // --- QTableWidget: 名称 | 类型 | 值 ---
    m_table = new QTableWidget(0, 3, this);
    m_table->setHorizontalHeaderLabels({
        QStringLiteral("名称"),
        QStringLiteral("类型"),
        QStringLiteral("值")
    });
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->verticalHeader()->setVisible(false);
    m_table->setShowGrid(false);
    m_table->setAlternatingRowColors(true);

    auto* hdr = m_table->horizontalHeader();
    hdr->setSectionResizeMode(0, QHeaderView::Stretch);   // 名称
    hdr->setSectionResizeMode(1, QHeaderView::ResizeToContents); // 类型
    hdr->setSectionResizeMode(2, QHeaderView::Stretch);   // 值

    m_table->setStyleSheet(R"(
        QTableWidget {
            font-family: 'Microsoft YaHei';
            font-size: 12px;
            color: #212121;
            background-color: #FFFFFF;
            border: 1px solid #E0E0E0;
            border-radius: 4px;
            gridline-color: #F0F0F0;
        }
        QTableWidget::item {
            padding: 4px 8px;
            border-bottom: 1px solid #F0F0F0;
        }
        QTableWidget::item:selected {
            background-color: #E3F2FD;
            color: #1565C0;
        }
        QHeaderView::section {
            background-color: #F5F7FA;
            font-family: 'Microsoft YaHei';
            font-size: 11px;
            font-weight: bold;
            color: #616161;
            padding: 4px 8px;
            border: none;
            border-bottom: 2px solid #E0E0E0;
        }
    )");
    layout->addWidget(m_table, 1);

    // --- Buttons row ---
    auto* btnLayout = new QHBoxLayout();
    btnLayout->setSpacing(4);

    QString btnStyle = R"(
        QPushButton {
            font-family: 'Microsoft YaHei';
            font-size: 11px;
            color: #2196F3;
            background: transparent;
            border: 1px dashed #BBDEFB;
            border-radius: 3px;
            padding: 4px 10px;
        }
        QPushButton:hover { background-color: #E3F2FD; }
    )";

    m_addBtn = new QPushButton(QStringLiteral("+ 添加变量"), this);
    m_addBtn->setStyleSheet(btnStyle);
    btnLayout->addWidget(m_addBtn);

    m_delBtn = new QPushButton(QStringLiteral("删除"), this);
    m_delBtn->setStyleSheet(btnStyle.replace("#2196F3", "#F44336")
                                 .replace("#BBDEFB", "#FFCDD2")
                                 .replace("#E3F2FD", "#FFEBEE"));
    m_delBtn->setEnabled(false);
    btnLayout->addWidget(m_delBtn);

    btnLayout->addStretch();
    layout->addLayout(btnLayout);

    // --- Connections ---
    connect(m_addBtn, &QPushButton::clicked,
            this, &VariableEditorPanel::onAddVariable);
    connect(m_delBtn, &QPushButton::clicked,
            this, &VariableEditorPanel::onDeleteVariable);
    connect(m_table, &QTableWidget::cellChanged,
            this, &VariableEditorPanel::onCellChanged);
    connect(m_table, &QTableWidget::itemSelectionChanged, this, [this]() {
        m_delBtn->setEnabled(m_table->currentRow() >= 0);
    });

    // --- Panel background ---
    setStyleSheet(R"(
        VariableEditorPanel {
            background-color: #FAFBFC;
        }
    )");
}

// ============================================================
// Set scope & refresh
// ============================================================

void VariableEditorPanel::setScope(VariableScope* scope)
{
    m_scope = scope;
    refreshTable();
}

void VariableEditorPanel::refreshTable()
{
    // Block signals to avoid triggering onCellChanged during population
    m_table->blockSignals(true);

    m_table->setRowCount(0);

    if (!m_scope) {
        m_table->blockSignals(false);
        return;
    }

    auto names = m_scope->names();
    m_table->setRowCount(static_cast<int>(names.size()));

    for (int i = 0; i < static_cast<int>(names.size()); ++i) {
        const std::string& name = names[i];
        VarType type = m_scope->type(name);

        // Column 0: 名称
        auto* nameItem = new QTableWidgetItem(QString::fromStdString(name));
        nameItem->setFlags(nameItem->flags() & ~Qt::ItemIsEditable);
        m_table->setItem(i, 0, nameItem);

        // Column 1: 类型 (combo box)
        auto* typeCombo = new QComboBox();
        typeCombo->addItems({QStringLiteral("Number"),
                             QStringLiteral("Boolean"),
                             QStringLiteral("String")});
        switch (type) {
            case VarType::Boolean: typeCombo->setCurrentIndex(1); break;
            case VarType::String:  typeCombo->setCurrentIndex(2); break;
            default:               typeCombo->setCurrentIndex(0); break;
        }
        typeCombo->setStyleSheet(R"(
            QComboBox {
                font-family: 'Microsoft YaHei'; font-size: 11px;
                color: #212121; background: #FFFFFF;
                border: 1px solid #D0D0D0; border-radius: 3px;
                padding: 2px 4px;
            }
        )");
        m_table->setCellWidget(i, 1, typeCombo);

        // Column 2: 值
        QString valueText;
        switch (type) {
            case VarType::Number: {
                auto v = m_scope->getNumber(name);
                valueText = v.has_value() ? QString::number(v.value()) : QStringLiteral("0");
                break;
            }
            case VarType::Boolean: {
                auto v = m_scope->getBool(name);
                valueText = v.has_value() ? (v.value() ? QStringLiteral("true") : QStringLiteral("false"))
                                          : QStringLiteral("false");
                break;
            }
            case VarType::String: {
                auto v = m_scope->getString(name);
                valueText = v.has_value() ? QString::fromStdString(v.value()) : QString();
                break;
            }
        }
        auto* valueItem = new QTableWidgetItem(valueText);
        m_table->setItem(i, 2, valueItem);
    }

    m_table->blockSignals(false);
}

// ============================================================
// Slots
// ============================================================

void VariableEditorPanel::onAddVariable()
{
    if (!m_scope) return;

    bool ok = false;
    QString name = QInputDialog::getText(this,
        QStringLiteral("添加变量"),
        QStringLiteral("变量名:"),
        QLineEdit::Normal,
        QString(),
        &ok);

    if (!ok || name.trimmed().isEmpty()) return;

    std::string nameStr = name.trimmed().toStdString();

    if (m_scope->has(nameStr)) {
        QMessageBox::warning(this,
            QStringLiteral("重复"),
            QStringLiteral("变量 \"%1\" 已存在。").arg(name.trimmed()));
        return;
    }

    m_scope->setNumber(nameStr, 0.0);
    refreshTable();
    emit variablesChanged();
}

void VariableEditorPanel::onDeleteVariable()
{
    if (!m_scope) return;

    int row = m_table->currentRow();
    if (row < 0) return;

    auto* nameItem = m_table->item(row, 0);
    if (!nameItem) return;

    std::string name = nameItem->text().toStdString();

    // VariableScope doesn't have a direct remove method,
    // but we can use clear + re-set for remaining vars.
    // Actually, VariableScope has no remove method. We'll remove via a workaround.
    // For now, just clear all and re-add except the deleted one.

    auto allNames = m_scope->names();
    // Store values we want to keep
    std::vector<std::pair<std::string, VarType>> keepVars;
    for (const auto& n : allNames) {
        if (n == name) continue;
        VarType t = m_scope->type(n);
        keepVars.push_back({n, t});
    }

    m_scope->clear();
    for (const auto& kv : keepVars) {
        switch (kv.second) {
            case VarType::Number:
                m_scope->setNumber(kv.first, 0.0);
                break;
            case VarType::Boolean:
                m_scope->setBool(kv.first, false);
                break;
            case VarType::String:
                m_scope->setString(kv.first, "");
                break;
        }
    }

    refreshTable();
    emit variablesChanged();
}

void VariableEditorPanel::onCellChanged(int row, int col)
{
    if (!m_scope || row < 0) return;

    auto* nameItem = m_table->item(row, 0);
    auto* valueItem = m_table->item(row, 2);
    auto* typeWidget = m_table->cellWidget(row, 1);

    if (!nameItem || !valueItem) return;

    std::string name = nameItem->text().toStdString();
    QString valueStr = valueItem->text().trimmed();

    // Determine type from combo box
    VarType type = VarType::Number;
    if (auto* combo = qobject_cast<QComboBox*>(typeWidget)) {
        switch (combo->currentIndex()) {
            case 1: type = VarType::Boolean; break;
            case 2: type = VarType::String;  break;
            default: type = VarType::Number; break;
        }
    }

    switch (type) {
        case VarType::Number: {
            bool ok = false;
            double v = valueStr.toDouble(&ok);
            if (ok) m_scope->setNumber(name, v);
            break;
        }
        case VarType::Boolean: {
            bool v = (valueStr.toLower() == QStringLiteral("true")
                      || valueStr == QStringLiteral("1"));
            m_scope->setBool(name, v);
            break;
        }
        case VarType::String: {
            m_scope->setString(name, valueStr.toStdString());
            break;
        }
    }

    emit variablesChanged();
}

} // namespace MotorStudio
