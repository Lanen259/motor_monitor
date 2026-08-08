#include "NodeParamPanel.h"

#include <QLineEdit>
#include <QSpinBox>
#include <QTextEdit>
#include <QComboBox>
#include <QTableWidget>
#include <QHeaderView>
#include <QPushButton>
#include <QGroupBox>
#include <QInputDialog>
#include <QHBoxLayout>
#include <algorithm>

namespace MotorStudio {

// ============================================================
// Helper — map param name to index in FlowNode::params
// ============================================================
static int paramIndex(const FlowNode& node, const QString& key)
{
    for (size_t i = 0; i < node.params.size(); ++i) {
        if (node.params[i].first == key.toStdString()) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

static QString paramValue(const FlowNode& node, const QString& key,
                          const QString& def = QString())
{
    int idx = paramIndex(node, key);
    return (idx >= 0) ? QString::fromStdString(node.params[idx].second) : def;
}

// ============================================================
// Helper — add a form row with label and widget
// ============================================================
template <typename WidgetT>
static WidgetT* addRow(QFormLayout* form, const QString& label,
                       const QString& tooltip = QString())
{
    auto* w = new WidgetT();
    w->setStyleSheet(R"(
        QLineEdit, QSpinBox, QComboBox {
            font-family: 'Microsoft YaHei';
            font-size: 12px;
            color: #212121;
            background-color: #FFFFFF;
            border: 1px solid #D0D0D0;
            border-radius: 3px;
            padding: 4px 6px;
            min-height: 24px;
        }
        QLineEdit:focus, QSpinBox:focus, QComboBox:focus {
            border-color: #2196F3;
        }
    )");

    auto* lbl = new QLabel(label);
    lbl->setStyleSheet(R"(
        QLabel {
            font-family: 'Microsoft YaHei';
            font-size: 12px;
            color: #424242;
            padding: 2px 0;
        }
    )");

    if (!tooltip.isEmpty()) {
        lbl->setToolTip(tooltip);
        w->setToolTip(tooltip);
    }

    form->addRow(lbl, w);
    return w;
}

static void addHintRow(QFormLayout* form, const QString& text)
{
    auto* hint = new QLabel(text);
    hint->setWordWrap(true);
    hint->setStyleSheet(R"(
        QLabel {
            font-family: 'Microsoft YaHei';
            font-size: 11px;
            color: #9E9E9E;
            font-style: italic;
            padding: 0 0 4px 0;
        }
    )");
    form->addRow(new QLabel(), hint);
}

// ============================================================
// Add / Delete parameter handlers
// ============================================================
void NodeParamPanel::onAddParamClicked()
{
    if (!m_currentNode) return;

    // Simple inline dialog using QInputDialog
    bool ok = false;
    QString key = QInputDialog::getText(this,
        QStringLiteral("添加参数"),
        QStringLiteral("参数名:"),
        QLineEdit::Normal,
        QString(),
        &ok);
    if (!ok || key.trimmed().isEmpty()) return;

    QString value = QInputDialog::getText(this,
        QStringLiteral("添加参数"),
        QStringLiteral("参数值:"),
        QLineEdit::Normal,
        QString(),
        &ok);
    if (!ok) return;

    // Check for duplicate key
    for (const auto& p : m_currentNode->params) {
        if (p.first == key.trimmed().toStdString()) {
            // Overwrite existing
            break;
        }
    }

    // Add to node params
    m_currentNode->params.emplace_back(key.trimmed().toStdString(), value.toStdString());

    // Rebuild form (buildForm manages m_buildingForm internally)
    buildForm(*m_currentNode);
    emit paramsChanged();
}

void NodeParamPanel::onDeleteParam(const QString& key)
{
    if (!m_currentNode) return;

    auto& params = m_currentNode->params;
    params.erase(std::remove_if(params.begin(), params.end(),
        [&key](const auto& p) { return p.first == key.toStdString(); }),
        params.end());

    // Rebuild form
    buildForm(*m_currentNode);
    emit paramsChanged();
}

// ============================================================
// Constructor
// ============================================================
NodeParamPanel::NodeParamPanel(QWidget* parent)
    : QWidget(parent)
{
    setupUi();
}

void NodeParamPanel::setupUi()
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // --- Title label ---
    m_titleLabel = new QLabel(QStringLiteral("节点参数"), this);
    m_titleLabel->setStyleSheet(R"(
        QLabel {
            font-family: 'Microsoft YaHei';
            font-size: 14px;
            font-weight: bold;
            color: #2196F3;
            padding: 10px 12px;
            border-bottom: 1px solid #E0E0E0;
        }
    )");
    layout->addWidget(m_titleLabel);

    // --- Scroll area for dynamic form ---
    m_scrollArea = new QScrollArea(this);
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setFrameShape(QFrame::NoFrame);
    m_scrollArea->setStyleSheet(R"(
        QScrollArea {
            background-color: #FFFFFF;
            border: none;
        }
        QScrollBar:vertical {
            background: #F5F5F5;
            width: 8px;
            border-radius: 4px;
        }
        QScrollBar::handle:vertical {
            background: #BDBDBD;
            border-radius: 4px;
            min-height: 20px;
        }
        QScrollBar::handle:vertical:hover {
            background: #9E9E9E;
        }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
            height: 0px;
        }
    )");

    // Form container widget
    m_formWidget = new QWidget();
    m_formLayout = new QFormLayout(m_formWidget);
    m_formLayout->setContentsMargins(12, 12, 12, 12);
    m_formLayout->setSpacing(8);
    m_formLayout->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);

    m_scrollArea->setWidget(m_formWidget);
    layout->addWidget(m_scrollArea, 1);

    // --- Empty state label ---
    m_emptyLabel = new QLabel(QStringLiteral("选择一个节点以编辑参数"), this);
    m_emptyLabel->setAlignment(Qt::AlignCenter);
    m_emptyLabel->setWordWrap(true);
    m_emptyLabel->setStyleSheet(R"(
        QLabel {
            font-family: 'Microsoft YaHei';
            font-size: 13px;
            color: #9E9E9E;
            padding: 24px 8px;
        }
    )");
    m_emptyLabel->hide();
    layout->addWidget(m_emptyLabel);

    // --- Panel background ---
    setStyleSheet(R"(
        NodeParamPanel {
            background-color: #FFFFFF;
        }
    )");
}

// ============================================================
// Set / clear node
// ============================================================
void NodeParamPanel::setNode(FlowNode* node)
{
    m_currentNode = node;

    if (!node) {
        clearNode();
        return;
    }

    m_scrollArea->show();
    m_emptyLabel->hide();

    buildForm(*node);
}

void NodeParamPanel::clearNode()
{
    m_currentNode = nullptr;
    clearForm();

    m_titleLabel->setText(QStringLiteral("节点参数"));
    m_scrollArea->hide();
    m_emptyLabel->show();
}

void NodeParamPanel::clearForm()
{
    while (m_formLayout->rowCount() > 0) {
        QLayoutItem* item = m_formLayout->takeAt(0);
        if (item) {
            if (item->widget()) {
                delete item->widget();
            }
            delete item;
        }
    }
}

// ============================================================
// Sync param changes back to FlowNode
// ============================================================
void NodeParamPanel::onAnyParamChanged()
{
    if (m_buildingForm || !m_currentNode) return;

    // Walk the form layout and sync values from widgets back to node params
    for (int i = 0; i < m_formLayout->rowCount(); ++i) {
        QLayoutItem* labelItem = m_formLayout->itemAt(i, QFormLayout::LabelRole);
        QLayoutItem* fieldItem = m_formLayout->itemAt(i, QFormLayout::FieldRole);
        if (!fieldItem) continue;

        QWidget* container = fieldItem->widget();
        if (!container) continue;

        // Find the actual field widget (may be nested in a container with delete button)
        QWidget* w = container;
        if (auto* innerLayout = container->layout()) {
            for (int j = 0; j < innerLayout->count(); ++j) {
                QLayoutItem* child = innerLayout->itemAt(j);
                if (child && child->widget()) {
                    QString pk = child->widget()->property("paramKey").toString();
                    if (!pk.isEmpty()) {
                        w = child->widget();
                        break;
                    }
                }
            }
        }

        QString key = w->property("paramKey").toString();
        if (key.isEmpty()) continue;
        // Skip internal keys (not real params)
        if (key.startsWith("_")) continue;
        // Skip generic table (handled by its own cellChanged lambda)
        if (qobject_cast<QTableWidget*>(w)) continue;

        QString value;

        if (auto* le = qobject_cast<QLineEdit*>(w)) {
            value = le->text();
        } else if (auto* sb = qobject_cast<QSpinBox*>(w)) {
            value = QString::number(sb->value());
        } else if (auto* te = qobject_cast<QTextEdit*>(w)) {
            value = te->toPlainText();
        } else if (auto* cb = qobject_cast<QComboBox*>(w)) {
            value = cb->currentText();
        }

        // Update or insert in node params
        bool found = false;
        for (auto& p : m_currentNode->params) {
            if (p.first == key.toStdString()) {
                p.second = value.toStdString();
                found = true;
                break;
            }
        }
        if (!found) {
            m_currentNode->params.emplace_back(key.toStdString(), value.toStdString());
        }
    }

    emit paramsChanged();
}

// ============================================================
// Dynamic form builder — per node type
// ============================================================
void NodeParamPanel::buildForm(const FlowNode& node)
{
    m_buildingForm = true;
    clearForm();

    // --- Title line: shows node type name ---
    QString typeName = QString::fromStdString(node.type);
    {
        QFont titleFont("Microsoft YaHei", 13, QFont::Bold);
        auto* typeLabel = new QLabel(QStringLiteral("类型: %1").arg(typeName));
        typeLabel->setFont(titleFont);
        typeLabel->setStyleSheet("color: #1565C0; padding: 4px 0;");
        m_formLayout->addRow(typeLabel);
    }

    // --- Common: node label ---
    {
        auto* lbl = new QLabel(QStringLiteral("标签"));
        lbl->setStyleSheet("font-family: 'Microsoft YaHei'; font-size: 12px; color: #424242;");
        auto* edit = new QLineEdit(QString::fromStdString(node.label));
        edit->setProperty("paramKey", "_label");
        edit->setStyleSheet(R"(
            QLineEdit {
                font-family: 'Microsoft YaHei';
                font-size: 12px;
                color: #212121;
                background-color: #FFFFFF;
                border: 1px solid #D0D0D0;
                border-radius: 3px;
                padding: 4px 6px;
            }
            QLineEdit:focus { border-color: #2196F3; }
        )");
        QObject::connect(edit, &QLineEdit::textChanged, this, [this, edit]() {
            if (m_currentNode) {
                m_currentNode->label = edit->text().toStdString();
            }
            onAnyParamChanged();
        });
        m_formLayout->addRow(lbl, edit);
    }

    // Separator
    {
        auto* sep = new QLabel(QStringLiteral("──── 参数 ────"));
        sep->setStyleSheet("font-family: 'Microsoft YaHei'; font-size: 11px; color: #9E9E9E; padding: 4px 0;");
        m_formLayout->addRow(sep);
    }

    // ================================================================
    // Per-type parameter editors
    // ================================================================

    // ---- 设置参数 (SetParameter) ----
    if (typeName == "SetParameter") {
        auto* nameEdit = addRow<QLineEdit>(m_formLayout, QStringLiteral("参数名"));
        nameEdit->setProperty("paramKey", "name");
        nameEdit->setText(paramValue(node, "name"));

        auto* valueEdit = addRow<QLineEdit>(m_formLayout, QStringLiteral("参数值"));
        valueEdit->setProperty("paramKey", "value");
        valueEdit->setText(paramValue(node, "value", "0"));

        auto* timeoutSpin = addRow<QSpinBox>(m_formLayout, QStringLiteral("超时(ms)"));
        timeoutSpin->setProperty("paramKey", "timeout");
        timeoutSpin->setRange(0, 60000);
        timeoutSpin->setValue(paramValue(node, "timeout", "5000").toInt());

        QObject::connect(nameEdit, &QLineEdit::textChanged,
                         this, &NodeParamPanel::onAnyParamChanged);
        QObject::connect(valueEdit, &QLineEdit::textChanged,
                         this, &NodeParamPanel::onAnyParamChanged);
        QObject::connect(timeoutSpin, QOverload<int>::of(&QSpinBox::valueChanged),
                         this, &NodeParamPanel::onAnyParamChanged);

        // Required field validation: pink background when empty
        auto applyNameValidation = [nameEdit]() {
            if (nameEdit->text().trimmed().isEmpty()) {
                nameEdit->setStyleSheet(R"(
                    QLineEdit {
                        font-family: 'Microsoft YaHei'; font-size: 12px;
                        color: #212121;
                        background-color: #FFEBEE;
                        border: 1px solid #EF9A9A;
                        border-radius: 3px; padding: 4px 6px; min-height: 24px;
                    }
                    QLineEdit:focus { border-color: #F44336; }
                )");
            } else {
                nameEdit->setStyleSheet(R"(
                    QLineEdit {
                        font-family: 'Microsoft YaHei'; font-size: 12px;
                        color: #212121;
                        background-color: #FFFFFF;
                        border: 1px solid #D0D0D0;
                        border-radius: 3px; padding: 4px 6px; min-height: 24px;
                    }
                    QLineEdit:focus { border-color: #2196F3; }
                )");
            }
        };
        QObject::connect(nameEdit, &QLineEdit::textChanged, this, applyNameValidation);
        applyNameValidation();

        auto applyValueValidation = [valueEdit]() {
            if (valueEdit->text().trimmed().isEmpty()) {
                valueEdit->setStyleSheet(R"(
                    QLineEdit {
                        font-family: 'Microsoft YaHei'; font-size: 12px;
                        color: #212121;
                        background-color: #FFEBEE;
                        border: 1px solid #EF9A9A;
                        border-radius: 3px; padding: 4px 6px; min-height: 24px;
                    }
                    QLineEdit:focus { border-color: #F44336; }
                )");
            } else {
                valueEdit->setStyleSheet(R"(
                    QLineEdit {
                        font-family: 'Microsoft YaHei'; font-size: 12px;
                        color: #212121;
                        background-color: #FFFFFF;
                        border: 1px solid #D0D0D0;
                        border-radius: 3px; padding: 4px 6px; min-height: 24px;
                    }
                    QLineEdit:focus { border-color: #2196F3; }
                )");
            }
        };
        QObject::connect(valueEdit, &QLineEdit::textChanged, this, applyValueValidation);
        applyValueValidation();
    }

    // ---- 启动电机 (StartMotor) ----
    else if (typeName == "StartMotor") {
        auto* modeCombo = addRow<QComboBox>(m_formLayout, QStringLiteral("启动模式"));
        modeCombo->setProperty("paramKey", "mode");
        modeCombo->addItems({QStringLiteral("位置"),
                             QStringLiteral("速度"),
                             QStringLiteral("力矩")});
        int idx = modeCombo->findText(paramValue(node, "mode", QStringLiteral("速度")));
        if (idx >= 0) modeCombo->setCurrentIndex(idx);

        auto* targetEdit = addRow<QLineEdit>(m_formLayout, QStringLiteral("目标值"));
        targetEdit->setProperty("paramKey", "target");
        targetEdit->setText(paramValue(node, "target", "0"));

        QObject::connect(modeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
                         this, &NodeParamPanel::onAnyParamChanged);
        QObject::connect(targetEdit, &QLineEdit::textChanged,
                         this, &NodeParamPanel::onAnyParamChanged);
    }

    // ---- 停止电机 (StopMotor) ----
    else if (typeName == "StopMotor") {
        auto* modeCombo = addRow<QComboBox>(m_formLayout, QStringLiteral("停止模式"));
        modeCombo->setProperty("paramKey", "mode");
        modeCombo->addItems({QStringLiteral("减速停止"),
                             QStringLiteral("急停"),
                             QStringLiteral("自由停机")});
        int idx = modeCombo->findText(paramValue(node, "mode", QStringLiteral("减速停止")));
        if (idx >= 0) modeCombo->setCurrentIndex(idx);

        QObject::connect(modeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
                         this, &NodeParamPanel::onAnyParamChanged);
    }

    // ---- 速度斜坡 (SpeedRamp) ----
    else if (typeName == "SpeedRamp") {
        auto* fromEdit = addRow<QLineEdit>(m_formLayout, QStringLiteral("起始速度(rpm)"));
        fromEdit->setProperty("paramKey", "from");
        fromEdit->setText(paramValue(node, "from", "0"));

        auto* toEdit = addRow<QLineEdit>(m_formLayout, QStringLiteral("目标速度(rpm)"));
        toEdit->setProperty("paramKey", "to");
        toEdit->setText(paramValue(node, "to", "1000"));

        auto* timeSpin = addRow<QSpinBox>(m_formLayout, QStringLiteral("斜坡时间(ms)"));
        timeSpin->setProperty("paramKey", "rampMs");
        timeSpin->setRange(0, 60000);
        timeSpin->setValue(paramValue(node, "rampMs", "1000").toInt());

        QObject::connect(fromEdit, &QLineEdit::textChanged,
                         this, &NodeParamPanel::onAnyParamChanged);
        QObject::connect(toEdit, &QLineEdit::textChanged,
                         this, &NodeParamPanel::onAnyParamChanged);
        QObject::connect(timeSpin, QOverload<int>::of(&QSpinBox::valueChanged),
                         this, &NodeParamPanel::onAnyParamChanged);
    }

    // ---- 自定义命令 (CustomCommand) ----
    else if (typeName == "CustomCommand") {
        auto* cmdEdit = addRow<QLineEdit>(m_formLayout, QStringLiteral("命令字符串"));
        cmdEdit->setProperty("paramKey", "command");
        cmdEdit->setText(paramValue(node, "command"));

        auto* timeoutSpin = addRow<QSpinBox>(m_formLayout, QStringLiteral("超时(ms)"));
        timeoutSpin->setProperty("paramKey", "timeout");
        timeoutSpin->setRange(100, 60000);
        timeoutSpin->setValue(paramValue(node, "timeout", "5000").toInt());

        QObject::connect(cmdEdit, &QLineEdit::textChanged,
                         this, &NodeParamPanel::onAnyParamChanged);
        QObject::connect(timeoutSpin, QOverload<int>::of(&QSpinBox::valueChanged),
                         this, &NodeParamPanel::onAnyParamChanged);
    }

    // ---- 延时 (Delay) ----
    else if (typeName == "Delay") {
        auto* delaySpin = addRow<QSpinBox>(m_formLayout, QStringLiteral("时长(ms)"));
        delaySpin->setProperty("paramKey", "ms");
        delaySpin->setRange(0, 60000);
        delaySpin->setValue(paramValue(node, "ms", "1000").toInt());

        QObject::connect(delaySpin, QOverload<int>::of(&QSpinBox::valueChanged),
                         this, &NodeParamPanel::onAnyParamChanged);
    }

    // ---- 等待条件 (Wait) ----
    else if (typeName == "Wait") {
        auto* condEdit = addRow<QLineEdit>(m_formLayout, QStringLiteral("等待条件"));
        condEdit->setProperty("paramKey", "condition");
        condEdit->setText(paramValue(node, "condition"));

        auto* timeoutSpin = addRow<QSpinBox>(m_formLayout, QStringLiteral("超时(ms)"));
        timeoutSpin->setProperty("paramKey", "timeout");
        timeoutSpin->setRange(100, 60000);
        timeoutSpin->setValue(paramValue(node, "timeout", "5000").toInt());

        addHintRow(m_formLayout, QStringLiteral("例: channel:Ia > 0.5"));

        QObject::connect(condEdit, &QLineEdit::textChanged,
                         this, &NodeParamPanel::onAnyParamChanged);
        QObject::connect(timeoutSpin, QOverload<int>::of(&QSpinBox::valueChanged),
                         this, &NodeParamPanel::onAnyParamChanged);
    }

    // ---- 定时器 (Timer) ----
    else if (typeName == "Timer") {
        auto* intervalSpin = addRow<QSpinBox>(m_formLayout, QStringLiteral("间隔(ms)"));
        intervalSpin->setProperty("paramKey", "interval");
        intervalSpin->setRange(10, 60000);
        intervalSpin->setValue(paramValue(node, "interval", "1000").toInt());

        auto* repeatCombo = addRow<QComboBox>(m_formLayout, QStringLiteral("重复"));
        repeatCombo->setProperty("paramKey", "repeat");
        repeatCombo->addItems({QStringLiteral("单次"),
                               QStringLiteral("无限"),
                               QStringLiteral("次数")});
        int idx = repeatCombo->findText(paramValue(node, "repeat", QStringLiteral("单次")));
        if (idx >= 0) repeatCombo->setCurrentIndex(idx);

        QObject::connect(intervalSpin, QOverload<int>::of(&QSpinBox::valueChanged),
                         this, &NodeParamPanel::onAnyParamChanged);
        QObject::connect(repeatCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
                         this, &NodeParamPanel::onAnyParamChanged);
    }

    // ---- 判断 (If) / 分支 (Switch) ----
    else if (typeName == "If" || typeName == "Switch") {
        auto* exprTe = addRow<QTextEdit>(m_formLayout, QStringLiteral("条件表达式"));
        exprTe->setProperty("paramKey", "expression");
        exprTe->setText(paramValue(node, "expression"));
        exprTe->setMaximumHeight(80);
        exprTe->setStyleSheet(R"(
            QTextEdit {
                font-family: 'Microsoft YaHei';
                font-size: 12px;
                color: #212121;
                background-color: #FFFFFF;
                border: 1px solid #D0D0D0;
                border-radius: 3px;
                padding: 4px 6px;
            }
            QTextEdit:focus { border-color: #2196F3; }
        )");

        addHintRow(m_formLayout,
                   QStringLiteral("例: $温度 < 85 && channel:Ia > 0"));

        QObject::connect(exprTe, &QTextEdit::textChanged,
                         this, &NodeParamPanel::onAnyParamChanged);
    }

    // ---- 循环 (While) ----
    else if (typeName == "While") {
        auto* loopCombo = addRow<QComboBox>(m_formLayout, QStringLiteral("循环类型"));
        loopCombo->setProperty("paramKey", "loopType");
        loopCombo->addItems({QStringLiteral("for"),
                             QStringLiteral("while"),
                             QStringLiteral("次数"),
                             QStringLiteral("无限")});
        int idx = loopCombo->findText(
            paramValue(node, "loopType", QStringLiteral("while")));
        if (idx >= 0) loopCombo->setCurrentIndex(idx);

        auto* condEdit = addRow<QLineEdit>(m_formLayout, QStringLiteral("条件/次数"));
        condEdit->setProperty("paramKey", "condition");
        condEdit->setText(paramValue(node, "condition"));

        auto* maxSpin = addRow<QSpinBox>(m_formLayout, QStringLiteral("上限"));
        maxSpin->setProperty("paramKey", "maxIterations");
        maxSpin->setRange(1, 999999);
        maxSpin->setValue(paramValue(node, "maxIterations", "1000").toInt());

        QObject::connect(loopCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
                         this, &NodeParamPanel::onAnyParamChanged);
        QObject::connect(condEdit, &QLineEdit::textChanged,
                         this, &NodeParamPanel::onAnyParamChanged);
        QObject::connect(maxSpin, QOverload<int>::of(&QSpinBox::valueChanged),
                         this, &NodeParamPanel::onAnyParamChanged);
    }

    // ---- 循环 for (For) ----
    else if (typeName == "For") {
        auto* varEdit = addRow<QLineEdit>(m_formLayout, QStringLiteral("变量名"));
        varEdit->setProperty("paramKey", "var");
        varEdit->setText(paramValue(node, "var", "i"));

        auto* fromSpin = addRow<QSpinBox>(m_formLayout, QStringLiteral("起始值"));
        fromSpin->setProperty("paramKey", "from");
        fromSpin->setRange(-999999, 999999);
        fromSpin->setValue(paramValue(node, "from", "0").toInt());

        auto* toSpin = addRow<QSpinBox>(m_formLayout, QStringLiteral("结束值"));
        toSpin->setProperty("paramKey", "to");
        toSpin->setRange(-999999, 999999);
        toSpin->setValue(paramValue(node, "to", "10").toInt());

        QObject::connect(varEdit, &QLineEdit::textChanged,
                         this, &NodeParamPanel::onAnyParamChanged);
        QObject::connect(fromSpin, QOverload<int>::of(&QSpinBox::valueChanged),
                         this, &NodeParamPanel::onAnyParamChanged);
        QObject::connect(toSpin, QOverload<int>::of(&QSpinBox::valueChanged),
                         this, &NodeParamPanel::onAnyParamChanged);
    }

    // ---- 跳转 (Jump) ----
    else if (typeName == "Jump") {
        auto* targetEdit = addRow<QLineEdit>(m_formLayout, QStringLiteral("跳转目标标签"));
        targetEdit->setProperty("paramKey", "target");
        targetEdit->setText(paramValue(node, "target"));

        QObject::connect(targetEdit, &QLineEdit::textChanged,
                         this, &NodeParamPanel::onAnyParamChanged);
    }

    // ---- 计算 (Calculate / Math / Expression) ----
    else if (typeName == "Calculate" || typeName == "Math"
             || typeName == "Expression") {
        auto* exprEdit = addRow<QLineEdit>(m_formLayout, QStringLiteral("表达式"));
        exprEdit->setProperty("paramKey", "expression");
        exprEdit->setText(paramValue(node, "expression"));

        addHintRow(m_formLayout,
                   QStringLiteral("例: (channel:Ia+channel:Ib+channel:Ic)/3"));

        QObject::connect(exprEdit, &QLineEdit::textChanged,
                         this, &NodeParamPanel::onAnyParamChanged);
    }

    // ---- 变量赋值 (AssignVariable) ----
    else if (typeName == "AssignVariable") {
        auto* varEdit = addRow<QLineEdit>(m_formLayout, QStringLiteral("变量名"));
        varEdit->setProperty("paramKey", "var");
        varEdit->setText(paramValue(node, "var"));

        auto* exprEdit = addRow<QLineEdit>(m_formLayout, QStringLiteral("表达式"));
        exprEdit->setProperty("paramKey", "expression");
        exprEdit->setText(paramValue(node, "expression"));

        addHintRow(m_formLayout,
                   QStringLiteral("例: (channel:Ia+channel:Ib+channel:Ic)/3"));

        QObject::connect(varEdit, &QLineEdit::textChanged,
                         this, &NodeParamPanel::onAnyParamChanged);
        QObject::connect(exprEdit, &QLineEdit::textChanged,
                         this, &NodeParamPanel::onAnyParamChanged);
    }

    // ---- 读参数 (ReadParameter) ----
    else if (typeName == "ReadParameter") {
        auto* nameEdit = addRow<QLineEdit>(m_formLayout, QStringLiteral("参数名"));
        nameEdit->setProperty("paramKey", "name");
        nameEdit->setText(paramValue(node, "name"));

        auto* timeoutSpin = addRow<QSpinBox>(m_formLayout, QStringLiteral("超时(ms)"));
        timeoutSpin->setProperty("paramKey", "timeout");
        timeoutSpin->setRange(100, 60000);
        timeoutSpin->setValue(paramValue(node, "timeout", "5000").toInt());

        QObject::connect(nameEdit, &QLineEdit::textChanged,
                         this, &NodeParamPanel::onAnyParamChanged);
        QObject::connect(timeoutSpin, QOverload<int>::of(&QSpinBox::valueChanged),
                         this, &NodeParamPanel::onAnyParamChanged);
    }

    // ---- 写寄存器 (WriteRegister) ----
    else if (typeName == "WriteRegister") {
        auto* addrEdit = addRow<QLineEdit>(m_formLayout, QStringLiteral("寄存器地址"));
        addrEdit->setProperty("paramKey", "address");
        addrEdit->setText(paramValue(node, "address", "0x00"));

        auto* valueEdit = addRow<QLineEdit>(m_formLayout, QStringLiteral("写入值"));
        valueEdit->setProperty("paramKey", "value");
        valueEdit->setText(paramValue(node, "value", "0"));

        QObject::connect(addrEdit, &QLineEdit::textChanged,
                         this, &NodeParamPanel::onAnyParamChanged);
        QObject::connect(valueEdit, &QLineEdit::textChanged,
                         this, &NodeParamPanel::onAnyParamChanged);
    }

    // ---- 发送命令 (SendCommand / SerialSend / CanSend) ----
    else if (typeName == "SendCommand" || typeName == "SerialSend"
             || typeName == "CanSend") {
        auto* cmdEdit = addRow<QLineEdit>(m_formLayout, QStringLiteral("命令"));
        cmdEdit->setProperty("paramKey", "command");
        cmdEdit->setText(paramValue(node, "command"));

        auto* timeoutSpin = addRow<QSpinBox>(m_formLayout, QStringLiteral("超时(ms)"));
        timeoutSpin->setProperty("paramKey", "timeout");
        timeoutSpin->setRange(100, 60000);
        timeoutSpin->setValue(paramValue(node, "timeout", "5000").toInt());

        QObject::connect(cmdEdit, &QLineEdit::textChanged,
                         this, &NodeParamPanel::onAnyParamChanged);
        QObject::connect(timeoutSpin, QOverload<int>::of(&QSpinBox::valueChanged),
                         this, &NodeParamPanel::onAnyParamChanged);
    }

    // ---- 记录数据 (RecordData) ----
    else if (typeName == "RecordData") {
        auto* nameEdit = addRow<QLineEdit>(m_formLayout, QStringLiteral("名称"));
        nameEdit->setProperty("paramKey", "name");
        nameEdit->setText(paramValue(node, "name"));

        auto* srcEdit = addRow<QLineEdit>(m_formLayout, QStringLiteral("来源/表达式"));
        srcEdit->setProperty("paramKey", "source");
        srcEdit->setText(paramValue(node, "source"));

        QObject::connect(nameEdit, &QLineEdit::textChanged,
                         this, &NodeParamPanel::onAnyParamChanged);
        QObject::connect(srcEdit, &QLineEdit::textChanged,
                         this, &NodeParamPanel::onAnyParamChanged);
    }

    // ---- 日志输出 (LogOutput) ----
    else if (typeName == "LogOutput") {
        auto* levelCombo = addRow<QComboBox>(m_formLayout, QStringLiteral("日志级别"));
        levelCombo->setProperty("paramKey", "level");
        levelCombo->addItems({QStringLiteral("INFO"),
                              QStringLiteral("WARN"),
                              QStringLiteral("ERROR"),
                              QStringLiteral("DEBUG")});
        int idx = levelCombo->findText(
            paramValue(node, "level", QStringLiteral("INFO")));
        if (idx >= 0) levelCombo->setCurrentIndex(idx);

        auto* msgEdit = addRow<QLineEdit>(m_formLayout, QStringLiteral("消息"));
        msgEdit->setProperty("paramKey", "message");
        msgEdit->setText(paramValue(node, "message"));

        QObject::connect(levelCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
                         this, &NodeParamPanel::onAnyParamChanged);
        QObject::connect(msgEdit, &QLineEdit::textChanged,
                         this, &NodeParamPanel::onAnyParamChanged);
    }

    // ---- 导出 (ExportData) ----
    else if (typeName == "ExportData") {
        auto* fmtCombo = addRow<QComboBox>(m_formLayout, QStringLiteral("导出格式"));
        fmtCombo->setProperty("paramKey", "format");
        fmtCombo->addItems({QStringLiteral("CSV"),
                            QStringLiteral("JSON"),
                            QStringLiteral("Excel")});
        int idx = fmtCombo->findText(
            paramValue(node, "format", QStringLiteral("CSV")));
        if (idx >= 0) fmtCombo->setCurrentIndex(idx);

        auto* pathEdit = addRow<QLineEdit>(m_formLayout, QStringLiteral("文件路径"));
        pathEdit->setProperty("paramKey", "path");
        pathEdit->setText(paramValue(node, "path"));

        QObject::connect(fmtCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
                         this, &NodeParamPanel::onAnyParamChanged);
        QObject::connect(pathEdit, &QLineEdit::textChanged,
                         this, &NodeParamPanel::onAnyParamChanged);
    }

    // ---- 断言 (Assert / AssertEqual / AssertGreater) ----
    else if (typeName == "Assert" || typeName == "AssertEqual"
             || typeName == "AssertGreater") {
        auto* condTe = addRow<QTextEdit>(m_formLayout, QStringLiteral("条件"));
        condTe->setProperty("paramKey", "expression");
        condTe->setText(paramValue(node, "expression"));
        condTe->setMaximumHeight(60);
        condTe->setStyleSheet(R"(
            QTextEdit {
                font-family: 'Microsoft YaHei';
                font-size: 12px;
                color: #212121;
                background-color: #FFFFFF;
                border: 1px solid #D0D0D0;
                border-radius: 3px;
                padding: 4px 6px;
            }
            QTextEdit:focus { border-color: #2196F3; }
        )");

        auto* msgEdit = addRow<QLineEdit>(m_formLayout, QStringLiteral("失败消息"));
        msgEdit->setProperty("paramKey", "message");
        msgEdit->setText(paramValue(node, "message"));

        QObject::connect(condTe, &QTextEdit::textChanged,
                         this, &NodeParamPanel::onAnyParamChanged);
        QObject::connect(msgEdit, &QLineEdit::textChanged,
                         this, &NodeParamPanel::onAnyParamChanged);
    }

    // ---- 异常处理 (ExceptionHandler / ThrowError / TryCatch / ErrorHandler) ----
    else if (typeName == "ExceptionHandler" || typeName == "ThrowError"
             || typeName == "TryCatch" || typeName == "ErrorHandler") {
        auto* msgEdit = addRow<QLineEdit>(m_formLayout, QStringLiteral("错误信息"));
        msgEdit->setProperty("paramKey", "message");
        msgEdit->setText(paramValue(node, "message"));

        auto* actionCombo = addRow<QComboBox>(m_formLayout, QStringLiteral("处理方式"));
        actionCombo->setProperty("paramKey", "action");
        actionCombo->addItems({QStringLiteral("中断流程"),
                               QStringLiteral("跳过"),
                               QStringLiteral("重试")});
        int idx = actionCombo->findText(
            paramValue(node, "action", QStringLiteral("中断流程")));
        if (idx >= 0) actionCombo->setCurrentIndex(idx);

        QObject::connect(msgEdit, &QLineEdit::textChanged,
                         this, &NodeParamPanel::onAnyParamChanged);
        QObject::connect(actionCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
                         this, &NodeParamPanel::onAnyParamChanged);
    }

    // ---- 子流程 (SubFlow) ----
    else if (typeName == "SubFlow") {
        auto* subflowCombo = addRow<QComboBox>(m_formLayout, QStringLiteral("子流程名称"));
        subflowCombo->setProperty("paramKey", "subflow");
        // Populated from FlowGraph::subGraphs at runtime by the caller.
        subflowCombo->addItem(
            paramValue(node, "subflow", QStringLiteral("(选择子流程)")));
        subflowCombo->setEditable(true);

        QObject::connect(subflowCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
                         this, &NodeParamPanel::onAnyParamChanged);
    }

    // ---- 注释 (Comment) ----
    else if (typeName == "Comment") {
        auto* commentTe = addRow<QTextEdit>(m_formLayout, QStringLiteral("注释内容"));
        commentTe->setProperty("paramKey", "text");
        commentTe->setText(paramValue(node, "text"));
        commentTe->setMinimumHeight(80);
        commentTe->setStyleSheet(R"(
            QTextEdit {
                font-family: 'Microsoft YaHei';
                font-size: 12px;
                color: #212121;
                background-color: #FFFFFF;
                border: 1px solid #D0D0D0;
                border-radius: 3px;
                padding: 4px 6px;
            }
            QTextEdit:focus { border-color: #2196F3; }
        )");

        QObject::connect(commentTe, &QTextEdit::textChanged,
                         this, &NodeParamPanel::onAnyParamChanged);
    }

    // ---- 存储值/获取值 (GetValue / StoreValue) ----
    else if (typeName == "GetValue" || typeName == "StoreValue") {
        auto* keyEdit = addRow<QLineEdit>(m_formLayout, QStringLiteral("键名"));
        keyEdit->setProperty("paramKey", "key");
        keyEdit->setText(paramValue(node, "key"));

        auto* valueEdit = addRow<QLineEdit>(m_formLayout, QStringLiteral("值"));
        valueEdit->setProperty("paramKey", "value");
        valueEdit->setText(paramValue(node, "value"));

        QObject::connect(keyEdit, &QLineEdit::textChanged,
                         this, &NodeParamPanel::onAnyParamChanged);
        QObject::connect(valueEdit, &QLineEdit::textChanged,
                         this, &NodeParamPanel::onAnyParamChanged);
    }

    // ---- 空操作 / 标签 (Nop / Label) ----
    else if (typeName == "Nop" || typeName == "Label") {
        auto* lblEdit = addRow<QLineEdit>(m_formLayout, QStringLiteral("标签"));
        lblEdit->setProperty("paramKey", "label");
        lblEdit->setText(paramValue(node, "label"));

        QObject::connect(lblEdit, &QLineEdit::textChanged,
                         this, &NodeParamPanel::onAnyParamChanged);
    }

    // ---- Fallback: generic key-value table for unknown types ----
    else {
        auto* genericLbl = new QLabel(QStringLiteral("通用参数 (键值对)"));
        genericLbl->setStyleSheet(R"(
            QLabel {
                font-family: 'Microsoft YaHei';
                font-size: 12px;
                font-weight: bold;
                color: #424242;
                padding: 4px 0;
            }
        )");
        m_formLayout->addRow(genericLbl);

        auto* table = new QTableWidget(this);
        table->setProperty("_genericTable", true);  // mark for signal throttling
        table->setColumnCount(2);
        table->setHorizontalHeaderLabels({QStringLiteral("键"),
                                          QStringLiteral("值")});
        table->horizontalHeader()->setStretchLastSection(true);
        table->horizontalHeader()->setDefaultSectionSize(100);
        table->setAlternatingRowColors(true);
        table->setStyleSheet(R"(
            QTableWidget {
                font-family: 'Microsoft YaHei';
                font-size: 12px;
                color: #212121;
                background-color: #FFFFFF;
                border: 1px solid #D0D0D0;
                gridline-color: #E8E8E8;
            }
            QTableWidget::item { padding: 2px 4px; }
            QHeaderView::section {
                background-color: #F5F7FA;
                font-family: 'Microsoft YaHei';
                font-size: 11px;
                font-weight: bold;
                color: #616161;
                padding: 4px 6px;
                border: 1px solid #E0E0E0;
            }
        )");

        // Populate from node params
        table->setRowCount(static_cast<int>(node.params.size()));
        for (size_t i = 0; i < node.params.size(); ++i) {
            auto* keyItem = new QTableWidgetItem(
                QString::fromStdString(node.params[i].first));
            keyItem->setFlags(keyItem->flags() & ~Qt::ItemIsEditable);
            table->setItem(static_cast<int>(i), 0, keyItem);

            auto* valItem = new QTableWidgetItem(
                QString::fromStdString(node.params[i].second));
            table->setItem(static_cast<int>(i), 1, valItem);
        }

        // Add-row button
        auto* addBtn = new QPushButton(QStringLiteral("+ 添加参数"), this);
        addBtn->setStyleSheet(R"(
            QPushButton {
                font-family: 'Microsoft YaHei';
                font-size: 11px;
                color: #2196F3;
                background: transparent;
                border: 1px dashed #BBDEFB;
                border-radius: 3px;
                padding: 3px 8px;
            }
            QPushButton:hover { background-color: #E3F2FD; }
        )");

        m_formLayout->addRow(table);
        m_formLayout->addRow(addBtn);

        QObject::connect(addBtn, &QPushButton::clicked, this, [this, table]() {
            int row = table->rowCount();
            table->insertRow(row);
            table->setItem(row, 0, new QTableWidgetItem(""));
            table->setItem(row, 1, new QTableWidgetItem(""));
            onAnyParamChanged();
        });

        QObject::connect(table, &QTableWidget::cellChanged, this,
                         [this, table](int row, int col) {
            if (m_buildingForm) return;
            // Sync from table back to node params
            if (m_currentNode && col == 1) {
                auto* keyItem = table->item(row, 0);
                auto* valItem = table->item(row, 1);
                if (keyItem && valItem) {
                    std::string key = keyItem->text().toStdString();
                    std::string val = valItem->text().toStdString();
                    bool found = false;
                    for (auto& p : m_currentNode->params) {
                        if (p.first == key) { p.second = val; found = true; break; }
                    }
                    if (!found && !key.empty()) {
                        m_currentNode->params.emplace_back(key, val);
                    }
                }
            }
            emit paramsChanged();
        });
    }

    // --- Wrap all param rows with delete buttons ---
    // Collect rows first, then process — avoid modifying layout during iteration
    struct RowInfo {
        int rowIndex;
        QString key;
        QWidget* field;
        QLabel* label;
    };
    std::vector<RowInfo> rowsToWrap;
    {
        for (int i = 0; i < m_formLayout->rowCount(); ++i) {
            QLayoutItem* labelItem = m_formLayout->itemAt(i, QFormLayout::LabelRole);
            QLayoutItem* fieldItem = m_formLayout->itemAt(i, QFormLayout::FieldRole);
            if (!fieldItem || !fieldItem->widget()) continue;

            QWidget* field = fieldItem->widget();
            QString key = field->property("paramKey").toString();
            if (key.isEmpty() || key.startsWith("_") || key == "_label") continue;
            if (qobject_cast<QTableWidget*>(field)) continue;
            if (qobject_cast<QPushButton*>(field)) continue;

            QLabel* lbl = qobject_cast<QLabel*>(labelItem ? labelItem->widget() : nullptr);
            rowsToWrap.push_back({i, key, field, lbl});
        }
    }
    // Process in reverse order to keep row indices stable
    for (auto it = rowsToWrap.rbegin(); it != rowsToWrap.rend(); ++it) {
        auto* container = new QWidget();
        auto* hbox = new QHBoxLayout(container);
        hbox->setContentsMargins(0, 0, 0, 0);
        hbox->setSpacing(2);
        hbox->addWidget(it->field, 1);

        auto* delBtn = new QPushButton(QStringLiteral("\xc3\x97"));
        delBtn->setFixedSize(20, 20);
        delBtn->setToolTip(QStringLiteral("删除此参数"));
        delBtn->setStyleSheet(R"(
            QPushButton {
                font-family: 'Microsoft YaHei'; font-size: 12px; font-weight: bold;
                color: #F44336; background: transparent;
                border: 1px solid transparent; border-radius: 3px; padding: 0;
            }
            QPushButton:hover {
                background-color: #FFEBEE; border-color: #EF9A9A;
            }
        )");
        hbox->addWidget(delBtn);

        QString captureKey = it->key;
        QObject::connect(delBtn, &QPushButton::clicked, this, [this, captureKey]() {
            onDeleteParam(captureKey);
        });

        // Collect old layout items BEFORE takeRow (they become inaccessible after)
        QLayoutItem* oldLabelLI = m_formLayout->itemAt(it->rowIndex, QFormLayout::LabelRole);
        QLayoutItem* oldFieldLI = m_formLayout->itemAt(it->rowIndex, QFormLayout::FieldRole);

        // takeRow detaches the row WITHOUT deleting items (unlike removeRow which
        // would delete them and cause double-free). The field widget is already
        // reparented into 'container' via hbox->addWidget above, so it won't leak.
        m_formLayout->takeRow(it->rowIndex);

        // Safe: takeRow did NOT delete these, so this is the ONLY delete (no double-free)
        delete oldLabelLI;
        delete oldFieldLI;

        if (it->label) {
            m_formLayout->insertRow(it->rowIndex, it->label, container);
        } else {
            m_formLayout->insertRow(it->rowIndex, QStringLiteral(""), container);
        }
    }

    // --- Fixed row: "+ 添加参数" button at bottom ---
    {
        auto* spacer = new QLabel();
        spacer->setFixedHeight(4);
        m_formLayout->addRow(spacer);

        auto* sep = new QLabel(QStringLiteral("──── 自定义参数 ────"));
        sep->setStyleSheet("font-family: 'Microsoft YaHei'; font-size: 11px; color: #9E9E9E; padding: 4px 0;");
        m_formLayout->addRow(sep);

        auto* addParamBtn = new QPushButton(QStringLiteral("+ 添加参数"), this);
        addParamBtn->setStyleSheet(R"(
            QPushButton {
                font-family: 'Microsoft YaHei'; font-size: 12px;
                color: #2196F3; background: transparent;
                border: 1px dashed #BBDEFB; border-radius: 4px;
                padding: 6px 12px;
            }
            QPushButton:hover { background-color: #E3F2FD; }
        )");
        QObject::connect(addParamBtn, &QPushButton::clicked,
                         this, &NodeParamPanel::onAddParamClicked);

        auto* btnRow = new QHBoxLayout();
        btnRow->setContentsMargins(0, 4, 0, 4);
        btnRow->addStretch();
        btnRow->addWidget(addParamBtn);
        btnRow->addStretch();
        m_formLayout->addRow(btnRow);
    }

    m_buildingForm = false;
}

} // namespace MotorStudio
