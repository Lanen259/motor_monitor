#pragma once

#include <QWidget>
#include <QVector>
#include <QLabel>
#include <QString>
#include <QGridLayout>

namespace MotorStudio {

// ============================================================
// 单个数值仪表盘
// ============================================================
class DashboardCell : public QWidget {
    Q_OBJECT
public:
    explicit DashboardCell(const QString& title, const QString& unit = "", QWidget* parent = nullptr);

    void setValue(float value);
    void setTitle(const QString& title);
    void setUnit(const QString& unit);
    void setWarningThreshold(float min, float max);

    float value() const { return m_value; }

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    QString m_title;
    QString m_unit;
    float m_value;
    float m_warnMin;
    float m_warnMax;
    QLabel* m_valueLabel;
    QLabel* m_titleLabel;
};

// ============================================================
// 仪表盘面板
// 管理多个 DashboardCell，支持网格布局
// ============================================================
class DashboardWidget : public QWidget {
    Q_OBJECT
public:
    explicit DashboardWidget(QWidget* parent = nullptr);

    // 单元格管理
    int addCell(const QString& title, const QString& unit = "");
    void removeCell(int index);
    void clearAll();
    int cellCount() const { return m_cells.size(); }

    // 批量更新
    void updateValues(const QVector<float>& values);
    void updateValue(int index, float value);

    // 设置警告阈值
    void setWarningThreshold(int index, float min, float max);

    // 布局
    void setColumns(int cols);

private:
    QVector<DashboardCell*> m_cells;
    QGridLayout* m_grid;
    int m_columns;
};

} // namespace MotorStudio