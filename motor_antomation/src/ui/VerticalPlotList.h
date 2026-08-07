#pragma once

#include <QScrollArea>
#include <QVector>
#include <QPushButton>
#include <QVBoxLayout>
#include <QLabel>

namespace MotorStudio {

class PlotCell;
class CurveEngine;

// VerticalPlotList — QScrollArea-based single-column multi-plot container.
//
// Each row is a self-contained PlotCell (header bar + channel bar + CurveWidget +
// resize handle).  Rows are laid out vertically with no extra drag handles —
// PlotCell owns its own resize handle.
//
// Part of WI-101/WI-102 integration (waveform interface round 7).
class VerticalPlotList : public QScrollArea {
    Q_OBJECT

public:
    explicit VerticalPlotList(CurveEngine* engine = nullptr, QWidget* parent = nullptr);
    ~VerticalPlotList() override;

    // Plot management
    int  addPlot(const QString& name = QString());
    void removePlot(int index);
    int  plotCount() const { return m_plotWidgets.size(); }
    PlotCell* plotAt(int index) const;
    const QVector<PlotCell*>& plotWidgets() const { return m_plotWidgets; }

    // CurveEngine attachment — attaches to all existing PlotCells' internal CurveWidgets
    void attachCurveEngine(CurveEngine* engine, int fps = 30);

    // Access highest-level container (for use in parent layouts)
    QWidget* toolbarWidget() const { return m_toolbarWidget; }

signals:
    void plotCountChanged(int count);

private slots:
    void onAddPlotClicked();
    void onClosePlot(int index);

private:
    void setupUi();
    void setupToolbar();

    // Toolbar
    QWidget*     m_toolbarWidget = nullptr;
    QPushButton* m_addPlotBtn    = nullptr;

    // Content area
    QWidget*     m_contentWidget = nullptr;
    QVBoxLayout* m_contentLayout = nullptr;

    // Plot data — each entry is a self-contained PlotCell
    QVector<PlotCell*> m_plotWidgets;

    // Naming counter
    int           m_plotCounter    = 1;

    // CurveEngine reference
    CurveEngine*  m_curveEngine = nullptr;
    int           m_fps         = 30;
};

} // namespace MotorStudio
