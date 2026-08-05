#include "FaultWidget.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QListWidget>
#include <QLabel>

namespace MotorStudio {

FaultWidget::FaultWidget(QWidget* parent)
    : QWidget(parent)
    , m_testMode(false)
{
    auto* layout = new QVBoxLayout(this);

    // 状态栏
    auto* statusBar = new QHBoxLayout();
    m_statusLabel = new QLabel(tr(" 无故障 "));
    m_statusLabel->setStyleSheet("QLabel { color: #00ff00; font-weight: bold; font-size: 14px; }");
    m_countLabel = new QLabel(tr("0 个活动故障"));
    m_countLabel->setStyleSheet("QLabel { color: #888; }");

    auto* clearBtn = new QPushButton(tr("清除全部"));
    statusBar->addWidget(m_statusLabel);
    statusBar->addStretch();
    statusBar->addWidget(m_countLabel);
    statusBar->addWidget(clearBtn);
    layout->addLayout(statusBar);

    // 故障列表
    m_faultList = new QListWidget();
    m_faultList->setStyleSheet("QListWidget { background-color: #1a1a2e; color: #ccc; border: 1px solid #333; }");
    layout->addWidget(m_faultList);

    connect(clearBtn, &QPushButton::clicked, this, &FaultWidget::clearAllFaults);
}

void FaultWidget::addFault(const FaultEntry& fault)
{
    m_faults.append(fault);

    // 测试模式下硬件故障 = 立即报警
    if (m_testMode && fault.isHardware) {
        emit hardwareFaultOccurred(fault);
    }

    emit faultAdded(fault);
    refreshDisplay();
}

void FaultWidget::clearFault(const QString& code)
{
    for (int i = 0; i < m_faults.size(); ++i) {
        if (m_faults[i].code == code) {
            m_faults[i].active = false;
        }
    }
    emit faultCleared(code);
    refreshDisplay();
}

void FaultWidget::clearAllFaults()
{
    m_faults.clear();
    refreshDisplay();
}

int FaultWidget::activeFaultCount() const
{
    int count = 0;
    for (const auto& f : m_faults) {
        if (f.active) count++;
    }
    return count;
}

bool FaultWidget::hasHardwareFault() const
{
    for (const auto& f : m_faults) {
        if (f.active && f.isHardware) return true;
    }
    return false;
}

void FaultWidget::setTestMode(bool testMode)
{
    m_testMode = testMode;
}

void FaultWidget::refreshDisplay()
{
    m_faultList->clear();

    int activeCount = 0;
    for (const auto& f : m_faults) {
        if (!f.active) continue;
        activeCount++;

        QString text = QString("[%1] %2: %3")
            .arg(f.timestamp.toString("HH:mm:ss"))
            .arg(f.code)
            .arg(f.message);

        auto* item = new QListWidgetItem(text);
        if (f.isHardware) {
            item->setForeground(Qt::red);
            item->setText("⚠ " + text);
        } else {
            item->setForeground(QColor(255, 165, 0));  // 橙色
        }
        m_faultList->addItem(item);
    }

    m_countLabel->setText(QString("%1 个活动故障").arg(activeCount));

    if (activeCount == 0) {
        m_statusLabel->setText(tr(" 无故障 "));
        m_statusLabel->setStyleSheet("QLabel { color: #00ff00; font-weight: bold; font-size: 14px; }");
    } else if (hasHardwareFault()) {
        m_statusLabel->setText(tr(" ⚠ 硬件故障 "));
        m_statusLabel->setStyleSheet("QLabel { color: #ff0000; font-weight: bold; font-size: 14px; }");
    } else {
        m_statusLabel->setText(tr(" 有故障 "));
        m_statusLabel->setStyleSheet("QLabel { color: #ff8800; font-weight: bold; font-size: 14px; }");
    }

    emit faultCountChanged(activeCount);
}

} // namespace MotorStudio