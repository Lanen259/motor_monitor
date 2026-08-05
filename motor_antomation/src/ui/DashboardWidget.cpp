#include "DashboardWidget.h"
#include <QPainter>
#include <QVBoxLayout>
#include <QFont>

namespace MotorStudio {

// ============================================================
// DashboardCell
// ============================================================

DashboardCell::DashboardCell(const QString& title, const QString& unit, QWidget* parent)
    : QWidget(parent)
    , m_title(title)
    , m_unit(unit)
    , m_value(0)
    , m_warnMin(-1e9f)
    , m_warnMax(1e9f)
{
    setMinimumSize(120, 80);
    setMaximumSize(250, 120);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 4, 8, 4);

    m_titleLabel = new QLabel(title, this);
    m_titleLabel->setAlignment(Qt::AlignCenter);
    m_titleLabel->setStyleSheet("QLabel { color: #888; font-size: 11px; }");

    m_valueLabel = new QLabel("--", this);
    m_valueLabel->setAlignment(Qt::AlignCenter);
    m_valueLabel->setStyleSheet("QLabel { color: #00bcd4; font-size: 24px; font-weight: bold; font-family: Consolas; }");

    layout->addWidget(m_titleLabel);
    layout->addWidget(m_valueLabel);

    setStyleSheet("DashboardCell { background-color: #16213e; border: 1px solid #2a2a4a; border-radius: 6px; }");
}

void DashboardCell::setValue(float value)
{
    m_value = value;

    QString text;
    if (m_unit.isEmpty()) {
        text = QString::number(value, 'f', 2);
    } else {
        text = QString("%1 %2").arg(value, 6, 'f', 2).arg(m_unit);
    }
    m_valueLabel->setText(text);

    // 警告/正常颜色
    if (value < m_warnMin || value > m_warnMax) {
        m_valueLabel->setStyleSheet("QLabel { color: #ff4444; font-size: 24px; font-weight: bold; font-family: Consolas; }");
    } else {
        m_valueLabel->setStyleSheet("QLabel { color: #00bcd4; font-size: 24px; font-weight: bold; font-family: Consolas; }");
    }
}

void DashboardCell::setTitle(const QString& title)
{
    m_title = title;
    m_titleLabel->setText(title);
}

void DashboardCell::setUnit(const QString& unit)
{
    m_unit = unit;
}

void DashboardCell::setWarningThreshold(float min, float max)
{
    m_warnMin = min;
    m_warnMax = max;
}

void DashboardCell::paintEvent(QPaintEvent* /*event*/)
{
    // 背景由样式表处理
}

// ============================================================
// DashboardWidget
// ============================================================

DashboardWidget::DashboardWidget(QWidget* parent)
    : QWidget(parent)
    , m_columns(4)
{
    m_grid = new QGridLayout(this);
    m_grid->setSpacing(6);
    m_grid->setContentsMargins(4, 4, 4, 4);
    setStyleSheet("background-color: #0f0f23;");
}

int DashboardWidget::addCell(const QString& title, const QString& unit)
{
    auto* cell = new DashboardCell(title, unit, this);
    int index = m_cells.size();
    m_cells.append(cell);

    int row = index / m_columns;
    int col = index % m_columns;
    m_grid->addWidget(cell, row, col);

    return index;
}

void DashboardWidget::removeCell(int index)
{
    if (index < 0 || index >= m_cells.size()) return;
    m_grid->removeWidget(m_cells[index]);
    delete m_cells[index];
    m_cells.removeAt(index);
}

void DashboardWidget::clearAll()
{
    for (auto* cell : m_cells) {
        m_grid->removeWidget(cell);
        delete cell;
    }
    m_cells.clear();
}

void DashboardWidget::updateValues(const QVector<float>& values)
{
    while (m_cells.size() < values.size()) {
        addCell(QString("CH%1").arg(m_cells.size() + 1));
    }

    for (int i = 0; i < values.size() && i < m_cells.size(); ++i) {
        m_cells[i]->setValue(values[i]);
    }
}

void DashboardWidget::updateValue(int index, float value)
{
    if (index >= 0 && index < m_cells.size()) {
        m_cells[index]->setValue(value);
    }
}

void DashboardWidget::setWarningThreshold(int index, float min, float max)
{
    if (index >= 0 && index < m_cells.size()) {
        m_cells[index]->setWarningThreshold(min, max);
    }
}

void DashboardWidget::setColumns(int cols)
{
    m_columns = std::max(1, cols);
    // 重新布局
    for (int i = m_cells.size() - 1; i >= 0; --i) {
        m_grid->removeWidget(m_cells[i]);
    }
    for (int i = 0; i < m_cells.size(); ++i) {
        int row = i / m_columns;
        int col = i % m_columns;
        m_grid->addWidget(m_cells[i], row, col);
    }
}

} // namespace MotorStudio