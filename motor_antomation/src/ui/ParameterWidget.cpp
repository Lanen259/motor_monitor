#include "parameter/ParameterManager.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QLineEdit>
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QPushButton>
#include <QScrollArea>
#include <QLabel>
#include <QFileDialog>
#include <QCheckBox>

namespace MotorStudio {

ParameterWidget::ParameterWidget(QWidget* parent)
    : QWidget(parent)
    , m_mgr(nullptr)
{
    auto* layout = new QVBoxLayout(this);

    // 工具栏
    auto* toolbar = new QHBoxLayout();
    auto* loadBtn = new QPushButton(tr("加载参数..."));
    auto* saveBtn = new QPushButton(tr("保存参数..."));
    auto* refreshBtn = new QPushButton(tr("刷新"));
    toolbar->addWidget(loadBtn);
    toolbar->addWidget(saveBtn);
    toolbar->addWidget(refreshBtn);
    toolbar->addStretch();
    layout->addLayout(toolbar);

    // 滚动区域
    auto* scroll = new QScrollArea();
    scroll->setWidgetResizable(true);
    m_contentWidget = new QWidget();
    scroll->setWidget(m_contentWidget);
    layout->addWidget(scroll);

    connect(loadBtn, &QPushButton::clicked, this, &ParameterWidget::loadFromFile);
    connect(saveBtn, &QPushButton::clicked, this, &ParameterWidget::saveToFile);
    connect(refreshBtn, &QPushButton::clicked, this, &ParameterWidget::refresh);
}

void ParameterWidget::setParameterManager(ParameterManager* mgr)
{
    m_mgr = mgr;
    refresh();
}

void ParameterWidget::refresh()
{
    if (!m_mgr) return;

    // 清除旧控件
    QLayoutItem* item;
    if (m_contentWidget->layout()) {
        while ((item = m_contentWidget->layout()->takeAt(0)) != nullptr) {
            delete item->widget();
            delete item;
        }
        delete m_contentWidget->layout();
    }

    auto* form = new QFormLayout(m_contentWidget);
    auto params = m_mgr->allParameters();

    for (int i = 0; i < params.size(); ++i) {
        const auto& p = params[i];
        QWidget* editor = nullptr;

        if (p.readOnly) {
            auto* label = new QLabel(p.value.toString());
            label->setStyleSheet("QLabel { color: gray; }");
            editor = label;
        } else {
            auto* spin = new QDoubleSpinBox();
            spin->setRange(p.minValue.toDouble(), p.maxValue.toDouble());
            spin->setValue(p.value.toDouble());
            spin->setDecimals(3);
            spin->setSuffix(" " + p.unit);
            int paramIdx = i;
            connect(spin, static_cast<void(QDoubleSpinBox::*)(double)>(&QDoubleSpinBox::valueChanged),
                    [this, paramIdx](double val) {
                if (m_mgr) m_mgr->setValue(paramIdx, val);
            });
            editor = spin;
        }

        QString label = QString("%1 (%2)").arg(p.displayName, p.name);
        form->addRow(label, editor);
    }
}

void ParameterWidget::saveToFile()
{
    if (!m_mgr) return;
    QString path = QFileDialog::getSaveFileName(this, tr("保存参数"), QString(), tr("JSON (*.json)"));
    if (!path.isEmpty()) m_mgr->saveToFile(path);
}

void ParameterWidget::loadFromFile()
{
    if (!m_mgr) return;
    QString path = QFileDialog::getOpenFileName(this, tr("加载参数"), QString(), tr("JSON (*.json)"));
    if (!path.isEmpty()) {
        m_mgr->loadFromFile(path);
        refresh();
    }
}

} // namespace MotorStudio