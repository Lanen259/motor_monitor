#include "MultiCurveContainer.h"
#include "CurveWidget.h"
#include "../curve/CurveEngine.h"

#include <QLabel>
#include <QInputDialog>
#include <QMessageBox>

namespace MotorStudio {

// ============================================================
// Construction
// ============================================================

MultiCurveContainer::MultiCurveContainer(CurveEngine* engine, QWidget* parent)
    : QWidget(parent)
    , m_curveEngine(engine)
{
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setContentsMargins(0, 0, 0, 0);
    m_mainLayout->setSpacing(0);

    // --- Toolbar ---
    setupToolbar();

    // --- Tab mode widget ---
    m_tabWidget = new QTabWidget();
    m_tabWidget->setTabsClosable(true);
    m_tabWidget->setMovable(true);
    m_tabWidget->setDocumentMode(true);
    m_tabWidget->setStyleSheet(
        "QTabWidget::pane {"
        "  background-color: #FFFFFF;"
        "  border: 1px solid #E0E0E0;"
        "}"
        "QTabBar::tab {"
        "  background-color: #F5F7FA;"
        "  color: #757575;"
        "  padding: 6px 16px;"
        "  border: 1px solid #E0E0E0;"
        "  border-bottom: none;"
        "  border-top-left-radius: 4px;"
        "  border-top-right-radius: 4px;"
        "  min-width: 80px;"
        "}"
        "QTabBar::tab:selected {"
        "  background-color: #FFFFFF;"
        "  color: #1976D2;"
        "  border-bottom: 2px solid #2196F3;"
        "}"
        "QTabBar::tab:hover:!selected {"
        "  background-color: #E3F2FD;"
        "}"
        "QTabBar::close-button {"
        "  image: none;"
        "  background: #BDBDBD;"
        "  border-radius: 2px;"
        "  margin-left: 4px;"
        "}"
        "QTabBar::close-button:hover {"
        "  background: #F44336;"
        "}"
    );

    connect(m_tabWidget, &QTabWidget::tabCloseRequested,
            this, &MultiCurveContainer::onTabCloseRequested);

    // Double-click on tab bar to rename
    m_tabWidget->tabBar()->installEventFilter(this);
    connect(m_tabWidget->tabBar(), &QTabBar::tabBarDoubleClicked,
            this, &MultiCurveContainer::onTabBarDoubleClicked);

    // --- Grid mode container ---
    m_gridContainer = new QWidget();
    m_gridContainer->setStyleSheet("background-color: #F5F7FA;");
    m_gridLayout = new QGridLayout(m_gridContainer);
    m_gridLayout->setContentsMargins(4, 4, 4, 4);
    m_gridLayout->setSpacing(2);

    // --- Add widgets to main layout ---
    m_mainLayout->addWidget(m_toolbarContainer);
    m_mainLayout->addWidget(m_tabWidget, 1);
    m_mainLayout->addWidget(m_gridContainer, 1);

    // Start in Tab mode with one default tab
    m_gridContainer->hide();
    addTab(tr("曲线 1"));
}

MultiCurveContainer::~MultiCurveContainer()
{
    // CurveWidgets are children of either tabWidget or gridContainer
    // and will be cleaned up automatically by Qt parent-child mechanism.
}

// ============================================================
// Toolbar
// ============================================================

void MultiCurveContainer::setupToolbar()
{
    m_toolbarContainer = new QWidget();
    m_toolbarContainer->setFixedHeight(36);
    m_toolbarContainer->setStyleSheet(
        "background-color: #FFFFFF; border-bottom: 1px solid #E0E0E0;"
    );

    m_toolbarLayout = new QHBoxLayout(m_toolbarContainer);
    m_toolbarLayout->setContentsMargins(8, 2, 8, 2);
    m_toolbarLayout->setSpacing(8);

    QLabel* modeLabel = new QLabel(tr("模式:"));
    modeLabel->setStyleSheet("color: #757575; font-size: 12px;");
    m_toolbarLayout->addWidget(modeLabel);

    m_modeCombo = new QComboBox();
    m_modeCombo->addItem(tr("标签页"), TabMode);
    m_modeCombo->addItem(tr("网格"), GridMode);
    m_modeCombo->setCurrentIndex(0);
    m_modeCombo->setFixedWidth(80);
    m_modeCombo->setStyleSheet(
        "QComboBox {"
        "  background-color: #FFFFFF; color: #212121;"
        "  border: 1px solid #E0E0E0; border-radius: 3px;"
        "  padding: 2px 8px; font-size: 12px;"
        "}"
        "QComboBox:hover { border: 1px solid #2196F3; }"
        "QComboBox QAbstractItemView {"
        "  background-color: #FFFFFF; color: #212121;"
        "  selection-background-color: #E3F2FD;"
        "}"
    );
    connect(m_modeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MultiCurveContainer::onModeComboChanged);
    m_toolbarLayout->addWidget(m_modeCombo);

    // Grid size selector (visible only in grid mode)
    m_gridLabel = new QLabel(tr("网格:"));
    m_gridLabel->setStyleSheet("color: #757575; font-size: 12px;");
    m_toolbarLayout->addWidget(m_gridLabel);

    m_gridSizeCombo = new QComboBox();
    m_gridSizeCombo->addItem(tr("2×2"), 0);   // index 0 = 2x2
    m_gridSizeCombo->addItem(tr("3×2"), 1);   // index 1 = 3x2
    m_gridSizeCombo->addItem(tr("4×3"), 2);   // index 2 = 4x3
    m_gridSizeCombo->setCurrentIndex(0);
    m_gridSizeCombo->setFixedWidth(70);
    m_gridSizeCombo->setStyleSheet(m_modeCombo->styleSheet());
    connect(m_gridSizeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MultiCurveContainer::onGridSizeComboChanged);
    m_toolbarLayout->addWidget(m_gridSizeCombo);

    // Add Tab button (visible only in tab mode)
    m_addTabBtn = new QPushButton(tr("+ 标签"));
    m_addTabBtn->setFixedHeight(24);
    m_addTabBtn->setFixedWidth(60);
    m_addTabBtn->setStyleSheet(
        "QPushButton {"
        "  background-color: #2196F3; color: #ffffff;"
        "  border: none; border-radius: 3px;"
        "  font-size: 12px; font-weight: bold;"
        "}"
        "QPushButton:hover { background-color: #42A5F5; }"
        "QPushButton:pressed { background-color: #1565C0; }"
    );
    connect(m_addTabBtn, &QPushButton::clicked,
            this, &MultiCurveContainer::onAddTabClicked);
    m_toolbarLayout->addWidget(m_addTabBtn);

    m_toolbarLayout->addStretch();

    // Initially grid controls are hidden
    m_gridSizeCombo->setVisible(false);
    m_gridLabel->setVisible(false);
}

// ============================================================
// Mode switching
// ============================================================

void MultiCurveContainer::setMode(Mode mode)
{
    if (m_mode == mode) return;
    m_mode = mode;

    // Block signals to avoid re-entrant calls during rebuild
    m_modeCombo->blockSignals(true);
    m_modeCombo->setCurrentIndex(static_cast<int>(mode));
    m_modeCombo->blockSignals(false);

    rebuildLayout();
    emit modeChanged(m_mode);
}

void MultiCurveContainer::onModeComboChanged(int index)
{
    Mode newMode = static_cast<Mode>(m_modeCombo->currentData().toInt());
    if (newMode == m_mode) return;
    m_mode = newMode;
    rebuildLayout();
    emit modeChanged(m_mode);
}

void MultiCurveContainer::setGridSize(int rows, int cols)
{
    if (m_gridRows == rows && m_gridCols == cols) return;
    m_gridRows = rows;
    m_gridCols = cols;

    // Update combo to match
    int comboIdx = -1;
    if (rows == 2 && cols == 2) comboIdx = 0;
    else if (rows == 3 && cols == 2) comboIdx = 1;
    else if (rows == 4 && cols == 3) comboIdx = 2;

    if (comboIdx >= 0) {
        m_gridSizeCombo->blockSignals(true);
        m_gridSizeCombo->setCurrentIndex(comboIdx);
        m_gridSizeCombo->blockSignals(false);
    }

    if (m_mode == GridMode) {
        rebuildLayout();
    }
    emit gridSizeChanged(m_gridRows, m_gridCols);
}

void MultiCurveContainer::onGridSizeComboChanged(int index)
{
    int rows, cols;
    switch (index) {
    case 0: rows = 2; cols = 2; break;
    case 1: rows = 3; cols = 2; break;
    case 2: rows = 4; cols = 3; break;
    default: rows = 2; cols = 2; break;
    }

    if (m_gridRows == rows && m_gridCols == cols) return;
    m_gridRows = rows;
    m_gridCols = cols;

    if (m_mode == GridMode) {
        rebuildLayout();
    }
    emit gridSizeChanged(m_gridRows, m_gridCols);
}

// ============================================================
// Layout rebuild — moves curve widgets between Tab and Grid
// ============================================================

void MultiCurveContainer::rebuildLayout()
{
    // Remove all curve widgets from current layout without deleting them
    QVector<CurveWidget*> widgets = m_curveWidgets;  // copy

    // Detach from tab widget
    while (m_tabWidget->count() > 0) {
        m_tabWidget->removeTab(0);
    }

    // Detach from grid layout
    while (m_gridLayout->count() > 0) {
        QLayoutItem* item = m_gridLayout->takeAt(0);
        // Don't delete the widget, just remove the layout item
        delete item;
    }

    // Ensure we have at least one curve widget
    if (widgets.isEmpty()) {
        widgets.append(createCurveWidget());
        m_curveWidgets = widgets;
    }

    if (m_mode == TabMode) {
        // Show tab widget, hide grid
        m_tabWidget->show();
        m_gridContainer->hide();

        // Show add-tab button and hide grid controls
        m_addTabBtn->setVisible(true);
        m_gridLabel->setVisible(false);
        m_gridSizeCombo->setVisible(false);

        // Add all widgets as tabs
        for (int i = 0; i < widgets.size(); ++i) {
            QString tabName = tr("曲线 %1").arg(i + 1);
            // Try to preserve previous names if any
            m_tabWidget->addTab(widgets[i], tabName);
        }
    } else {
        // Grid mode
        m_tabWidget->hide();
        m_gridContainer->show();

        // Hide add-tab button and show grid controls
        m_addTabBtn->setVisible(false);
        m_gridLabel->setVisible(true);
        m_gridSizeCombo->setVisible(true);

        int totalCells = m_gridRows * m_gridCols;

        // Ensure we have enough curve widgets
        while (widgets.size() < totalCells) {
            widgets.append(createCurveWidget());
        }
        m_curveWidgets = widgets;

        // Place in grid
        for (int i = 0; i < totalCells && i < widgets.size(); ++i) {
            int r = i / m_gridCols;
            int c = i % m_gridCols;
            m_gridLayout->addWidget(widgets[i], r, c);
        }
    }

    emit curveWidgetCountChanged(m_curveWidgets.size());
    emit tabCountChanged(m_tabWidget->count());
}

// ============================================================
// Curve widget factory
// ============================================================

CurveWidget* MultiCurveContainer::createCurveWidget()
{
    CurveWidget* cw = new CurveWidget();
    cw->setMinimumSize(150, 100);

    if (m_curveEngine) {
        cw->attachCurveEngine(m_curveEngine, m_fps);
    }

    // Connect time-axis change for grid sync
    connect(cw, &CurveWidget::timeAxisChanged, this, &MultiCurveContainer::onCurveTimeAxisChanged);

    return cw;
}

// ============================================================
// Tab operations
// ============================================================

int MultiCurveContainer::addTab(const QString& name)
{
    CurveWidget* cw = createCurveWidget();
    m_curveWidgets.append(cw);

    QString tabName = name.isEmpty()
        ? tr("曲线 %1").arg(m_tabCounter++)
        : name;

    if (m_mode == TabMode) {
        int idx = m_tabWidget->addTab(cw, tabName);
        m_tabWidget->setCurrentIndex(idx);
        emit tabCountChanged(m_tabWidget->count());
        emit curveWidgetCountChanged(m_curveWidgets.size());
        return idx;
    }

    // In grid mode, rebuild to include the new widget
    rebuildLayout();
    emit curveWidgetCountChanged(m_curveWidgets.size());
    return m_curveWidgets.size() - 1;
}

void MultiCurveContainer::removeTab(int index)
{
    if (m_curveWidgets.isEmpty()) return;

    // Clamp index
    if (index < 0 || index >= m_curveWidgets.size()) return;

    // Don't remove the last curve widget — curve widgets must remain >= 1
    if (m_curveWidgets.size() <= 1) return;

    CurveWidget* cw = m_curveWidgets.takeAt(index);

    if (m_mode == TabMode) {
        // Find and remove the tab containing this widget
        for (int i = 0; i < m_tabWidget->count(); ++i) {
            if (m_tabWidget->widget(i) == cw) {
                m_tabWidget->removeTab(i);
                break;
            }
        }
    }

    // Schedule deletion (safe even in grid mode where it may still be in layout)
    cw->detachCurveEngine();
    cw->deleteLater();

    if (m_mode == GridMode) {
        rebuildLayout();
    }

    emit tabCountChanged(m_tabWidget->count());
    emit curveWidgetCountChanged(m_curveWidgets.size());
}

void MultiCurveContainer::renameTab(int index, const QString& name)
{
    if (m_mode != TabMode) return;
    if (index < 0 || index >= m_tabWidget->count()) return;

    m_tabWidget->setTabText(index, name);
}

void MultiCurveContainer::onAddTabClicked()
{
    addTab();
}

void MultiCurveContainer::onTabCloseRequested(int index)
{
    // Map tab index to curve widget index
    if (index < 0 || index >= m_tabWidget->count()) return;

    QWidget* w = m_tabWidget->widget(index);
    int curveIdx = m_curveWidgets.indexOf(static_cast<CurveWidget*>(w));
    if (curveIdx >= 0) {
        removeTab(curveIdx);
    }
}

void MultiCurveContainer::onTabBarDoubleClicked(int index)
{
    if (m_mode != TabMode) return;
    if (index < 0 || index >= m_tabWidget->count()) return;

    bool ok = false;
    QString currentName = m_tabWidget->tabText(index);
    QString newName = QInputDialog::getText(
        this,
        tr("重命名标签"),
        tr("标签名称:"),
        QLineEdit::Normal,
        currentName,
        &ok
    );

    if (ok && !newName.isEmpty()) {
        m_tabWidget->setTabText(index, newName);
    }
}

// ============================================================
// Time-axis synchronization (Grid mode)
// ============================================================

void MultiCurveContainer::onCurveTimeAxisChanged()
{
    if (m_mode != GridMode) return;

    CurveWidget* source = qobject_cast<CurveWidget*>(sender());
    if (!source) return;

    syncTimeAxis(source);
}

void MultiCurveContainer::syncTimeAxis(CurveWidget* source)
{
    if (m_curveWidgets.size() < 2) return;

    uint64_t srcT0 = source->timeBase();
    double srcXRange = source->xRangeSeconds();

    // Propagate to all other curve widgets
    for (CurveWidget* cw : m_curveWidgets) {
        if (cw == source) continue;
        // Block signals to avoid infinite feedback loop
        cw->blockSignals(true);
        cw->setTimeBase(srcT0);
        cw->setXRangeSeconds(srcXRange);
        cw->blockSignals(false);
    }
}

// ============================================================
// Curve widget access
// ============================================================

CurveWidget* MultiCurveContainer::curveWidgetAt(int index) const
{
    if (index < 0 || index >= m_curveWidgets.size()) return nullptr;
    return m_curveWidgets[index];
}

CurveWidget* MultiCurveContainer::currentCurveWidget() const
{
    if (m_curveWidgets.isEmpty()) return nullptr;

    if (m_mode == TabMode) {
        QWidget* w = m_tabWidget->currentWidget();
        if (w) return qobject_cast<CurveWidget*>(w);
    }

    return m_curveWidgets.first();
}

// ============================================================
// CurveEngine attachment
// ============================================================

void MultiCurveContainer::attachCurveEngine(CurveEngine* engine, int fps)
{
    m_curveEngine = engine;
    m_fps = fps;

    for (CurveWidget* cw : m_curveWidgets) {
        cw->attachCurveEngine(engine, fps);
    }
}

} // namespace MotorStudio
