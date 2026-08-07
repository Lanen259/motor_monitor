#pragma once

#include <QWidget>
#include <QTableWidget>
#include <QComboBox>
#include <QPushButton>
#include <QLabel>
#include <QCheckBox>
#include <QVector>
#include <cstdint>
#include "../databus/Topic.h"

namespace MotorStudio {

class MultiCurveContainer;
class CurveWidget;
class CurveEngine;

// CurveManagerPanel — channel property management table for the Oscilloscope page.
// Displays all registered channels with inline editing for name, color, Y range,
// visibility toggle, and delete.  Integrates with MultiCurveContainer for target
// window selection and propagates changes to all CurveWidget instances.
class CurveManagerPanel : public QWidget {
    Q_OBJECT
public:
    explicit CurveManagerPanel(QWidget* parent = nullptr);

    // Hooks for external wiring
    void setCurveContainer(MultiCurveContainer* container);
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

    void applyColorToAllCurves(int row, const QColor& color);
    void applyVisibilityToAllCurves(int row, bool visible);
    void applyDeleteToAllCurves(const QString& channelName);

    QTableWidget* m_table = nullptr;
    QComboBox* m_windowCombo = nullptr;
    QPushButton* m_addBtn = nullptr;
    QPushButton* m_refreshBtn = nullptr;
    QLabel* m_titleLabel = nullptr;

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

    MultiCurveContainer* m_curveContainer = nullptr;
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
