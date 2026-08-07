#pragma once

#include <QWidget>
#include <QVector>
#include <QComboBox>
#include <QPushButton>
#include <QTabWidget>
#include <QTabBar>
#include <QGridLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>

namespace MotorStudio {

class CurveWidget;
class CurveEngine;

// MultiCurveContainer — multi-window curve system with Tab and Grid modes.
//
// Tab mode:  QTabBar at top, each tab holds an independent CurveWidget.
//            Tabs can be added (auto-create empty CurveWidget, switch to it),
//            closed (release CurveWidget resources), and renamed (double-click).
//
// Grid mode: QGridLayout with configurable rows x cols (2x2 / 3x2 / 4x3).
//            Each cell holds an independent CurveWidget.
//            All cells share a synchronized time axis (scroll/zoom one = all follow).
//
// CurveWidgets are shared between modes — switching mode preserves data.
class MultiCurveContainer : public QWidget {
    Q_OBJECT

public:
    enum Mode {
        TabMode = 0,
        GridMode
    };

    explicit MultiCurveContainer(CurveEngine* engine = nullptr, QWidget* parent = nullptr);
    ~MultiCurveContainer() override;

    // Mode control
    void setMode(Mode mode);
    Mode mode() const { return m_mode; }

    // Grid size
    void setGridSize(int rows, int cols);
    int gridRows() const { return m_gridRows; }
    int gridCols() const { return m_gridCols; }

    // Curve widget access
    int curveWidgetCount() const { return m_curveWidgets.size(); }
    CurveWidget* curveWidgetAt(int index) const;
    CurveWidget* currentCurveWidget() const;

    // Tab operations (convenience, also accessible via direct QTabWidget access)
    int addTab(const QString& name = QString());
    void removeTab(int index);
    void renameTab(int index, const QString& name);
    QTabWidget* tabWidget() const { return m_tabWidget; }

    // CurveEngine attachment — attaches to all existing and future CurveWidgets
    void attachCurveEngine(CurveEngine* engine, int fps = 30);

    // Access all curve widgets
    const QVector<CurveWidget*>& curveWidgets() const { return m_curveWidgets; }

signals:
    void modeChanged(Mode mode);
    void gridSizeChanged(int rows, int cols);
    void tabCountChanged(int count);
    void curveWidgetCountChanged(int count);

private slots:
    void onAddTabClicked();
    void onTabCloseRequested(int index);
    void onTabBarDoubleClicked(int index);
    void onModeComboChanged(int index);
    void onGridSizeComboChanged(int index);

    // Grid mode: time-axis sync — when one curve widget pans/zooms, others follow
    void onCurveTimeAxisChanged();

private:
    void rebuildLayout();
    void setupToolbar();
    void syncTimeAxis(CurveWidget* source);
    CurveWidget* createCurveWidget();

    // Mode & grid state
    Mode m_mode = TabMode;
    int m_gridRows = 2;
    int m_gridCols = 2;

    // Layout
    QVBoxLayout* m_mainLayout = nullptr;
    QWidget* m_toolbarContainer = nullptr;
    QHBoxLayout* m_toolbarLayout = nullptr;

    // Toolbar widgets
    QComboBox* m_modeCombo = nullptr;
    QLabel* m_gridLabel = nullptr;
    QComboBox* m_gridSizeCombo = nullptr;
    QPushButton* m_addTabBtn = nullptr;

    // Tab mode
    QTabWidget* m_tabWidget = nullptr;

    // Grid mode
    QWidget* m_gridContainer = nullptr;
    QGridLayout* m_gridLayout = nullptr;

    // All curve widgets (shared across modes)
    QVector<CurveWidget*> m_curveWidgets;

    // CurveEngine reference
    CurveEngine* m_curveEngine = nullptr;
    int m_fps = 30;

    // Tab counter for default naming
    int m_tabCounter = 1;
};

} // namespace MotorStudio
