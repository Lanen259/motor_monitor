#pragma once

#include <QWidget>
#include <QTableWidget>
#include <QComboBox>
#include <QPushButton>
#include <QHBoxLayout>
#include <QLabel>
#include <QCheckBox>
#include <QVector>
#include <cstdint>
#include "../databus/Topic.h"

namespace MotorStudio {

class VerticalPlotList;
class CurveWidget;
class CurveEngine;

// CurveManagerPanel — channel property management table for the Oscilloscope page.
// Displays all registered channels with inline editing for name, color, Y range,
// visibility toggle, and delete.  Integrates with VerticalPlotList for target
// plot cell selection and propagates changes to all PlotCell CurveWidgets.
class CurveManagerPanel : public QWidget {
    Q_OBJECT
public:
    explicit CurveManagerPanel(QWidget* parent = nullptr);

    // Hooks for external wiring
    void setPlotList(VerticalPlotList* plotList);
    void setCurveEngine(CurveEngine* engine);

    // Reload table from TopicRegistry + current CurveWidget state
    void refresh();

private slots:
    void onCellChanged(int row, int col);
    void onCellDoubleClicked(int row, int col);
    void onAddChannel();
    void onRemoveChannel(int row);
    void onVisibleToggled(int row, bool visible);
    void onTargetWindowChanged(int index);

private:
    void setupUi();
    void loadFromRegistry();
    CurveWidget* currentCurveWidget() const;

    // WI-104: 缩放工具栏按钮
    void setupZoomToolbar(QHBoxLayout* toolbar);

    void applyColorToAllCurves(int row, const QColor& color);
    void applyVisibilityToAllCurves(int row, bool visible);
    void applyDeleteToAllCurves(const QString& channelName);

    QTableWidget* m_table = nullptr;
    QComboBox* m_windowCombo = nullptr;
    QPushButton* m_addBtn = nullptr;
    QPushButton* m_refreshBtn = nullptr;
    QLabel* m_titleLabel = nullptr;

    // WI-104: 缩放控制按钮
    QPushButton* m_zoomInBtn = nullptr;
    QPushButton* m_zoomOutBtn = nullptr;
    QPushButton* m_autoFitBtn = nullptr;
    QPushButton* m_resetViewBtn = nullptr;

    struct ChannelRow {
        TopicId topicId = 0;
        QString name;
        QString unit;
        QColor color;
        float yMin = -10.0f;
        float yMax = 10.0f;
        bool visible = true;
    };
    QVector<ChannelRow> m_rows;

    VerticalPlotList* m_plotList = nullptr;
    CurveEngine* m_curveEngine = nullptr;

    static const int COL_NAME    = 0;
    static const int COL_COLOR   = 1;
    static const int COL_UNIT    = 2;
    static const int COL_YMIN    = 3;
    static const int COL_YMAX    = 4;
    static const int COL_VISIBLE = 5;
    static const int COL_ACTION  = 6;
    static const int COL_COUNT   = 7;
};

} // namespace MotorStudio
