#include "VerticalPlotList.h"
#include "PlotCell.h"
#include "CurveWidget.h"
#include "../curve/CurveEngine.h"
#include "../curve/TimeAxisManager.h"

#include <QHBoxLayout>
#include <QVBoxLayout>

namespace MotorStudio {

// ============================================================
// Construction
// ============================================================

VerticalPlotList::VerticalPlotList(CurveEngine* engine, QWidget* parent)
    : QScrollArea(parent)
    , m_curveEngine(engine)
{
    setupUi();
    setupToolbar();

    // No default plot added here — the caller (mainwindow) does it.
}

VerticalPlotList::~VerticalPlotList()
{
    // PlotCells are children of the content widget and will be
    // cleaned up by Qt parent-child mechanism.
}

// ============================================================
// UI setup
// ============================================================

void VerticalPlotList::setupUi()
{
    // Scroll area appearance
    setWidgetResizable(true);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    setStyleSheet(
        "QScrollArea {"
        "  background-color: #F5F7FA;"
        "  border: none;"
        "}"
    );

    // Content widget wraps toolbar + plot rows
    auto* outerWidget = new QWidget();
    auto* outerLayout = new QVBoxLayout(outerWidget);
    outerLayout->setContentsMargins(0, 0, 0, 0);
    outerLayout->setSpacing(0);

    // Toolbar placeholder — created in setupToolbar
    m_toolbarWidget = new QWidget();
    outerLayout->addWidget(m_toolbarWidget);

    // Separator line below toolbar
    auto* toolSep = new QFrame();
    toolSep->setFrameShape(QFrame::HLine);
    toolSep->setStyleSheet("QFrame { color: #E0E0E0; max-height: 1px; }");
    outerLayout->addWidget(toolSep);

    // Scrollable content area for PlotCell rows
    m_contentWidget = new QWidget();
    m_contentWidget->setStyleSheet("background-color: #F5F7FA;");
    m_contentLayout = new QVBoxLayout(m_contentWidget);
    m_contentLayout->setContentsMargins(8, 4, 8, 8);
    m_contentLayout->setSpacing(6);  // spacing between PlotCells

    outerLayout->addWidget(m_contentWidget, 1);

    setWidget(outerWidget);
}

void VerticalPlotList::setupToolbar()
{
    auto* layout = new QHBoxLayout(m_toolbarWidget);
    layout->setContentsMargins(12, 6, 12, 6);
    layout->setSpacing(8);

    m_toolbarWidget->setStyleSheet(
        "background-color: #FFFFFF; border-bottom: 1px solid #E0E0E0;"
    );

    auto* titleLabel = new QLabel(tr("子图列表"));
    titleLabel->setStyleSheet(
        "color: #1976D2; font-size: 13px; font-weight: bold;"
        "font-family: 'Microsoft YaHei';"
    );
    layout->addWidget(titleLabel);

    layout->addStretch();

    m_addPlotBtn = new QPushButton(tr("+ 新增子图"));
    m_addPlotBtn->setFixedHeight(28);
    m_addPlotBtn->setFixedWidth(90);
    m_addPlotBtn->setStyleSheet(
        "QPushButton {"
        "  background-color: #2196F3; color: #FFFFFF;"
        "  border: none; border-radius: 4px;"
        "  font-size: 12px; font-weight: bold;"
        "  font-family: 'Microsoft YaHei';"
        "}"
        "QPushButton:hover { background-color: #42A5F5; }"
        "QPushButton:pressed { background-color: #1565C0; }"
    );
    connect(m_addPlotBtn, &QPushButton::clicked,
            this, &VerticalPlotList::onAddPlotClicked);
    layout->addWidget(m_addPlotBtn);
}

// ============================================================
// Plot management
// ============================================================

int VerticalPlotList::addPlot(const QString& name)
{
    QString plotName = name.isEmpty()
        ? tr("曲线 %1").arg(m_plotCounter++)
        : name;

    PlotCell* cell = new PlotCell(plotName, m_curveEngine, m_contentWidget);
    m_plotWidgets.append(cell);

    // Insert into layout (before the trailing stretch)
    int insertIdx = m_contentLayout->count();
    m_contentLayout->insertWidget(insertIdx, cell);

    // Connect PlotCell's closeRequested to removePlot
    connect(cell, &PlotCell::closeRequested, this, [this, cell]() {
        int idx = m_plotWidgets.indexOf(cell);
        if (idx >= 0) onClosePlot(idx);
    });

    // Connect timeSync toggle to TimeAxisManager
    auto& taMgr = TimeAxisManager::instance();
    connect(cell, &PlotCell::timeSyncChanged, this, [cell](bool synced) {
        cell->curveWidget()->setTimeSynced(synced);
        if (synced) {
            TimeRange range = TimeAxisManager::instance().sharedRange();
            // Update CurveWidget from shared range
            cell->curveWidget()->setTimeAxisManager(&TimeAxisManager::instance());
        }
    });

    // Set the initial TimeAxisManager on the PlotCell's CurveWidget
    cell->curveWidget()->setTimeAxisManager(&taMgr);

    emit plotCountChanged(m_plotWidgets.size());
    return m_plotWidgets.size() - 1;
}

void VerticalPlotList::removePlot(int index)
{
    if (index < 0 || index >= m_plotWidgets.size()) return;
    if (m_plotWidgets.size() <= 1) return;  // keep at least one

    PlotCell* cell = m_plotWidgets.takeAt(index);
    cell->curveWidget()->detachCurveEngine();
    m_contentLayout->removeWidget(cell);
    cell->deleteLater();

    emit plotCountChanged(m_plotWidgets.size());
}

PlotCell* VerticalPlotList::plotAt(int index) const
{
    if (index < 0 || index >= m_plotWidgets.size()) return nullptr;
    return m_plotWidgets[index];
}

void VerticalPlotList::attachCurveEngine(CurveEngine* engine, int fps)
{
    m_curveEngine = engine;
    m_fps = fps;
    for (PlotCell* cell : m_plotWidgets) {
        cell->curveWidget()->attachCurveEngine(engine, fps);
    }
}

// ============================================================
// Slots
// ============================================================

void VerticalPlotList::onAddPlotClicked()
{
    addPlot();
}

void VerticalPlotList::onClosePlot(int index)
{
    removePlot(index);
}

} // namespace MotorStudio
