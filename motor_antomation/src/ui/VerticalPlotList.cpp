#include "VerticalPlotList.h"
#include "PlotCell.h"
#include "CurveWidget.h"
#include "../curve/CurveEngine.h"
#include "../curve/TimeAxisManager.h"

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QEvent>
#include <QWheelEvent>
#include <QMouseEvent>
#include <QScrollBar>

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

    // 安装 viewport 事件过滤器：
    //  - 吞掉 Wheel，使整个区域不再随滚轮滚动（滚轮只用于子图内缩放）
    //  - 非框选模式下左键拖拽滚动整个区域
    viewport()->installEventFilter(this);

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

    // 框选缩放模式开关：开启后左键在图上拖拽框选放大；
    // 关闭时（默认）左键拖拽用于滚动整个区域
    m_rubberBandBtn = new QPushButton(tr("框选"));
    m_rubberBandBtn->setCheckable(true);
    m_rubberBandBtn->setFixedHeight(28);
    m_rubberBandBtn->setFixedWidth(52);
    m_rubberBandBtn->setCursor(Qt::PointingHandCursor);
    m_rubberBandBtn->setToolTip(tr("框选缩放模式：开启后左键在图上拖拽框选放大；关闭时左键拖拽滚动整个区域"));
    m_rubberBandBtn->setStyleSheet(
        "QPushButton {"
        "  background-color: #FFFFFF; color: #1976D2;"
        "  border: 1px solid #90CAF9; border-radius: 4px;"
        "  font-size: 12px; font-weight: bold;"
        "  font-family: 'Microsoft YaHei';"
        "}"
        "QPushButton:hover { background-color: #E3F2FD; }"
        "QPushButton:checked {"
        "  background-color: #2196F3; color: #FFFFFF;"
        "  border: none;"
        "}"
        "QPushButton:checked:hover { background-color: #42A5F5; }"
    );
    connect(m_rubberBandBtn, &QPushButton::toggled,
            this, &VerticalPlotList::onRubberBandToggled);
    layout->addWidget(m_rubberBandBtn);

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

    // 应用当前框选模式到新子图
    cell->curveWidget()->setRubberBandEnabled(m_rubberBandMode);

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

void VerticalPlotList::onRubberBandToggled(bool checked)
{
    m_rubberBandMode = checked;
    applyRubberBandMode();
}

void VerticalPlotList::applyRubberBandMode()
{
    for (PlotCell* cell : m_plotWidgets) {
        cell->curveWidget()->setRubberBandEnabled(m_rubberBandMode);
    }
}

// ============================================================
// Viewport event filter (WI-105: 解耦滚轮滚动与子图缩放)
// ============================================================

bool VerticalPlotList::eventFilter(QObject* obj, QEvent* event)
{
    if (obj != viewport()) {
        return QScrollArea::eventFilter(obj, event);
    }

    switch (event->type()) {
    case QEvent::Wheel:
        // 整个区域禁用滚轮滚动：滚轮事件由子图 CurveWidget 自己 accept，
        // 未被子图处理的滚轮（头部/空白处）在这里被吞掉，不再滚动区域
        return true;

    case QEvent::MouseButtonPress: {
        auto* me = static_cast<QMouseEvent*>(event);
        if (me->button() == Qt::LeftButton && !m_rubberBandMode) {
            // 非框选模式：记录按下位置，待移动超过阈值后才真正开始拖拽滚动。
            // 这里不 grabMouse 也不吞掉事件，保证单击/双击（如双击自适应）不受影响。
            m_dragArmed = true;
            m_dragArmPos = me->globalPos();
            m_dragStartScrollValue = verticalScrollBar()->value();
        }
        return false;
    }

    case QEvent::MouseMove: {
        auto* me = static_cast<QMouseEvent*>(event);
        if (m_dragScrolling) {
            int dy = me->globalPos().y() - m_dragArmPos.y();
            verticalScrollBar()->setValue(m_dragStartScrollValue - dy);
            return true;
        }
        if (m_dragArmed && (me->buttons() & Qt::LeftButton)) {
            // 移动超过阈值才进入拖拽滚动，避免误吞单击/双击
            if (qAbs(me->globalPos().y() - m_dragArmPos.y()) > 4) {
                m_dragScrolling = true;
                viewport()->setCursor(Qt::ClosedHandCursor);
                viewport()->grabMouse();  // 确保 move/release 持续送达
                verticalScrollBar()->setValue(
                    m_dragStartScrollValue - (me->globalPos().y() - m_dragArmPos.y()));
                return true;
            }
        }
        return false;
    }

    case QEvent::MouseButtonRelease: {
        auto* me = static_cast<QMouseEvent*>(event);
        if (me->button() == Qt::LeftButton && m_dragScrolling) {
            m_dragScrolling = false;
            m_dragArmed = false;
            viewport()->releaseMouse();
            viewport()->unsetCursor();
            return true;
        }
        if (me->button() == Qt::LeftButton && m_dragArmed) {
            // 按下后未发生拖拽：视为普通点击，复位待命状态
            m_dragArmed = false;
        }
        return false;
    }

    default:
        return false;
    }
}

} // namespace MotorStudio
