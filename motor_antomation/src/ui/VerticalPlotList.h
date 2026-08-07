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

protected:
    // Viewport event filter: swallows wheel (no area scroll on wheel),
    // and implements left-drag scrolling of the whole area.
    bool eventFilter(QObject* obj, QEvent* event) override;

private slots:
    void onAddPlotClicked();
    void onClosePlot(int index);
    void onRubberBandToggled(bool checked);

private:
    void setupUi();
    void setupToolbar();
    void applyRubberBandMode();

    // Toolbar
    QWidget*     m_toolbarWidget = nullptr;
    QPushButton* m_addPlotBtn    = nullptr;
    QPushButton* m_rubberBandBtn = nullptr;

    // Interaction state
    bool m_rubberBandMode   = false;  // toolbar toggle: left-drag = rubber-band zoom
    bool m_dragArmed        = false;  // left pressed, waiting to exceed drag threshold
    bool m_dragScrolling    = false;  // actively drag-scrolling the whole area
    QPoint m_dragArmPos;              // global pos of left press (drag arm point)
    int  m_dragStartScrollValue = 0;

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
