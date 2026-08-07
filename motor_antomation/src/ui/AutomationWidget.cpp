#include "AutomationWidget.h"
#include "automation/AutomationEngine.h"
#include "FlowCanvas.h"
#include "NodeLibraryPanel.h"
#include "NodeParamPanel.h"
#include "automation/FlowRunner.h"
#include "automation/FlowGraph.h"
#include "automation/VariableScope.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSplitter>
#include <QHeaderView>
#include <QFileDialog>
#include <QMessageBox>
#include <QDateTime>
#include <QJsonDocument>
#include <QThread>
#include <QDebug>
#include <sstream>

namespace MotorStudio {

// ============================================================
// FlowRunWorker — helper that lives on a worker thread and
//                 calls FlowRunner::run() without blocking UI
// ============================================================
namespace {

class FlowRunWorker : public QObject {
    Q_OBJECT
public:
    FlowRunner* runner = nullptr;
    FlowGraph graph;
    ExecutionContext ctx;

public slots:
    void doRun() {
        if (runner) {
            runner->run(graph, ctx);
        }
    }
};

} // anonymous namespace

// ============================================================
// Helper: StepType to display string
// ============================================================
static QString stepTypeLabel(StepType t)
{
    switch (t) {
    case StepType::SetParameter: return QStringLiteral("设置");
    case StepType::Wait:         return QStringLiteral("等待");
    case StepType::ReadParameter: return QStringLiteral("读取");
    case StepType::Assert:       return QStringLiteral("断言");
    case StepType::RecordData:   return QStringLiteral("记录");
    case StepType::StartMotor:   return QStringLiteral("启动");
    case StepType::StopMotor:    return QStringLiteral("停止");
    case StepType::Custom:       return QStringLiteral("自定义");
    }
    return QStringLiteral("???");
}

// ============================================================
// Helper: build a compact params string (e.g. "Speed=1000, dur=100ms")
// ============================================================
static QString paramsSummary(const TestStep& step)
{
    QStringList parts;
    for (const auto& kv : step.params) {
        parts.append(QString::fromStdString(kv.first + "=" + kv.second));
    }
    return parts.join(", ");
}

// ============================================================
// Constructor / Destructor
// ============================================================

AutomationWidget::AutomationWidget(AutomationEngine* engine, QWidget* parent)
    : QWidget(parent), m_engine(engine)
{
    setupUi();
    applyDarkTheme();
    if (m_engine) {
        connectEngine(m_engine);
    }
    updateButtonStates(false, false);

    // Default to flowchart view
    if (m_viewStack) {
        m_viewStack->setCurrentIndex(1);  // flowchart page
        m_toggleViewBtn->setText(QStringLiteral("表格视图"));
    }
}

AutomationWidget::~AutomationWidget()
{
    // Stop any running flow
    if (m_flowRunner) {
        m_flowRunner->stop();
    }
}

// ============================================================
// Engine signal wiring
// ============================================================

void AutomationWidget::connectEngine(AutomationEngine* engine)
{
    m_engine = engine;
    if (!m_engine) return;

    connect(m_engine, &AutomationEngine::testStarted,
            this, &AutomationWidget::onTestStarted);
    connect(m_engine, &AutomationEngine::testCompleted,
            this, &AutomationWidget::onTestCompleted);
    connect(m_engine, &AutomationEngine::stepStarted,
            this, &AutomationWidget::onStepStarted);
    connect(m_engine, &AutomationEngine::stepCompleted,
            this, &AutomationWidget::onStepCompleted);
    connect(m_engine, &AutomationEngine::progressUpdated,
            this, &AutomationWidget::onProgressUpdated);
    connect(m_engine, &AutomationEngine::logMessage,
            this, &AutomationWidget::onLogMessage);
}

// ============================================================
// UI construction — main layout with QStackedWidget
// ============================================================

void AutomationWidget::setupUi()
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(12, 12, 12, 12);
    mainLayout->setSpacing(8);

    // ---- Row 0: toolbar (shared) ----
    auto* topBar = new QHBoxLayout();
    topBar->setSpacing(8);

    m_caseNameLabel = new QLabel(tr("未加载测试用例"));
    m_caseNameLabel->setStyleSheet("color: #2196F3; font-size: 14px; font-weight: bold;");
    topBar->addWidget(m_caseNameLabel);

    m_statusLabel = new QLabel(tr("空闲"));
    m_statusLabel->setStyleSheet("color: #757575; font-size: 13px;");
    topBar->addWidget(m_statusLabel);

    topBar->addStretch();

    // View toggle button
    m_toggleViewBtn = new QPushButton(tr("表格视图"));
    m_toggleViewBtn->setFixedHeight(30);
    m_toggleViewBtn->setStyleSheet(R"(
        QPushButton {
            background-color: #F5F7FA; color: #2196F3;
            border: 1px solid #BBDEFB; border-radius: 4px;
            padding: 4px 12px; font-size: 12px; font-weight: bold;
        }
        QPushButton:hover { background-color: #E3F2FD; }
    )");
    topBar->addWidget(m_toggleViewBtn);

    m_loadBtn = new QPushButton(tr("加载测试..."));
    m_loadBtn->setFixedHeight(30);
    topBar->addWidget(m_loadBtn);

    m_runBtn = new QPushButton(tr("运行"));
    m_runBtn->setFixedHeight(30);
    m_runBtn->setEnabled(false);
    topBar->addWidget(m_runBtn);

    m_pauseBtn = new QPushButton(tr("暂停"));
    m_pauseBtn->setFixedHeight(30);
    m_pauseBtn->setEnabled(false);
    topBar->addWidget(m_pauseBtn);

    m_resumeBtn = new QPushButton(tr("继续"));
    m_resumeBtn->setFixedHeight(30);
    m_resumeBtn->setEnabled(false);
    topBar->addWidget(m_resumeBtn);

    m_stopBtn = new QPushButton(tr("停止"));
    m_stopBtn->setFixedHeight(30);
    m_stopBtn->setEnabled(false);
    topBar->addWidget(m_stopBtn);

    mainLayout->addLayout(topBar);

    // ---- Row 1: Progress bar (shared) ----
    m_progressBar = new QProgressBar();
    m_progressBar->setFixedHeight(8);
    m_progressBar->setTextVisible(false);
    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(0);
    mainLayout->addWidget(m_progressBar);

    // ---- Row 2: QStackedWidget (table page + flowchart page) ----
    m_viewStack = new QStackedWidget();

    // Page 0: Table-based UI (existing)
    auto* tablePage = new QWidget();
    setupTableUi(tablePage);
    m_viewStack->addWidget(tablePage);  // index 0

    // Page 1: Flowchart IDE layout (WI-207)
    auto* flowPage = new QWidget();
    setupFlowUi(flowPage);
    m_viewStack->addWidget(flowPage);   // index 1

    mainLayout->addWidget(m_viewStack, 1);

    // ---- Row 3: Execution log (shared) ----
    auto* logTitle = new QLabel(tr("执行日志"));
    logTitle->setStyleSheet("color: #2196F3; font-size: 12px; font-weight: bold;");
    mainLayout->addWidget(logTitle);

    m_stepLog = new QPlainTextEdit();
    m_stepLog->setReadOnly(true);
    m_stepLog->setMaximumBlockCount(2000);
    m_stepLog->setFixedHeight(150);
    mainLayout->addWidget(m_stepLog);

    // ---- Row 4: Summary label (shared) ----
    m_summaryLabel = new QLabel();
    m_summaryLabel->setStyleSheet("color: #757575; font-size: 12px; padding: 4px 0;");
    m_summaryLabel->setVisible(false);
    mainLayout->addWidget(m_summaryLabel);

    // ---- Connections (toolbar buttons) ----
    m_loadBtn->setEnabled(true);  // Load is always enabled unless running
    connect(m_toggleViewBtn, &QPushButton::clicked, this, &AutomationWidget::onToggleView);

    // Default: connect to flowchart mode slots (will be re-wired on toggle)
    connect(m_loadBtn,  &QPushButton::clicked, this, &AutomationWidget::onLoadFlowGraph);
    connect(m_runBtn,   &QPushButton::clicked, this, &AutomationWidget::onRunFlowGraph);
    connect(m_stopBtn,  &QPushButton::clicked, this, &AutomationWidget::onStop);
    connect(m_pauseBtn, &QPushButton::clicked, this, &AutomationWidget::onPause);
    connect(m_resumeBtn,&QPushButton::clicked, this, &AutomationWidget::onResume);
}

// ============================================================
// Table-based UI page (existing code, extracted)
// ============================================================

void AutomationWidget::setupTableUi(QWidget* page)
{
    auto* pageLayout = new QVBoxLayout(page);
    pageLayout->setContentsMargins(0, 0, 0, 0);
    pageLayout->setSpacing(0);

    // QSplitter — step table (left) + detail panel (right)
    auto* splitter = new QSplitter(Qt::Horizontal);

    // — Left: QTableWidget —
    m_stepTable = new QTableWidget(0, 5); // #, Type, Params, Status, Duration
    m_stepTable->setHorizontalHeaderLabels({tr("步骤"), tr("类型"), tr("参数"), tr("状态"), tr("耗时")});
    m_stepTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_stepTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_stepTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_stepTable->verticalHeader()->setVisible(false);
    m_stepTable->setShowGrid(false);

    // Column widths
    auto* hdr = m_stepTable->horizontalHeader();
    hdr->setSectionResizeMode(0, QHeaderView::ResizeToContents);  // #
    hdr->setSectionResizeMode(1, QHeaderView::ResizeToContents);  // Type
    hdr->setSectionResizeMode(2, QHeaderView::Stretch);           // Params
    hdr->setSectionResizeMode(3, QHeaderView::ResizeToContents);  // Status
    hdr->setSectionResizeMode(4, QHeaderView::ResizeToContents);  // Duration

    splitter->addWidget(m_stepTable);

    // — Right: Step detail QGroupBox —
    m_detailGroup = new QGroupBox(tr("步骤详情"));
    m_detailLayout = new QFormLayout(m_detailGroup);
    m_detailLayout->setContentsMargins(12, 16, 12, 12);
    m_detailLayout->setSpacing(8);

    m_detailTypeLabel    = new QLabel(QStringLiteral("-"));
    m_detailDescLabel    = new QLabel(QStringLiteral("-"));
    m_detailTimeoutLabel = new QLabel(QStringLiteral("-"));
    m_detailRetryLabel   = new QLabel(QStringLiteral("-"));

    auto* detailTypeLbl    = new QLabel(tr("类型:"));
    auto* detailDescLbl    = new QLabel(tr("描述:"));
    auto* detailTimeoutLbl = new QLabel(tr("超时:"));
    auto* detailRetryLbl   = new QLabel(tr("重试次数:"));

    QString detailLabelStyle = "color: #212121; font-size: 12px;";
    QString detailValueStyle = "color: #757575; font-size: 12px; font-weight: bold;";

    detailTypeLbl->setStyleSheet(detailLabelStyle);
    detailDescLbl->setStyleSheet(detailLabelStyle);
    detailTimeoutLbl->setStyleSheet(detailLabelStyle);
    detailRetryLbl->setStyleSheet(detailLabelStyle);
    m_detailTypeLabel->setStyleSheet(detailValueStyle);
    m_detailDescLabel->setStyleSheet(detailValueStyle);
    m_detailTimeoutLabel->setStyleSheet(detailValueStyle);
    m_detailRetryLabel->setStyleSheet(detailValueStyle);

    m_detailLayout->addRow(detailTypeLbl,    m_detailTypeLabel);
    m_detailLayout->addRow(detailDescLbl,    m_detailDescLabel);
    m_detailLayout->addRow(detailTimeoutLbl, m_detailTimeoutLabel);
    m_detailLayout->addRow(detailRetryLbl,   m_detailRetryLabel);

    m_detailGroup->setMinimumWidth(240);
    splitter->addWidget(m_detailGroup);

    splitter->setStretchFactor(0, 3);
    splitter->setStretchFactor(1, 1);
    pageLayout->addWidget(splitter, 1);

    // Step selection
    connect(m_stepTable, &QTableWidget::itemSelectionChanged,
            this, &AutomationWidget::onStepSelected);
}

// ============================================================
// Flowchart IDE layout (WI-207)
// ============================================================

void AutomationWidget::setupFlowUi(QWidget* page)
{
    auto* pageLayout = new QHBoxLayout(page);
    pageLayout->setContentsMargins(0, 0, 0, 0);
    pageLayout->setSpacing(4);

    // --- Left: NodeLibraryPanel (fixed 220px) ---
    m_flowCanvas = new FlowCanvas();
    m_nodeLibrary = new NodeLibraryPanel(m_flowCanvas);
    m_nodeLibrary->setFixedWidth(220);
    pageLayout->addWidget(m_nodeLibrary);

    // --- Center: FlowCanvas (stretch=1) ---
    m_flowCanvas->setMinimumWidth(400);
    pageLayout->addWidget(m_flowCanvas, 1);

    // --- Right: NodeParamPanel (fixed 280px) ---
    m_paramPanel = new NodeParamPanel();
    m_paramPanel->setFixedWidth(280);
    pageLayout->addWidget(m_paramPanel);

    // --- Connections ---
    connect(m_flowCanvas, &FlowCanvas::nodeSelected,
            this, &AutomationWidget::onFlowNodeSelected);
    connect(m_flowCanvas, &FlowCanvas::nodeDeselected,
            this, &AutomationWidget::onFlowNodeDeselected);
}

// ============================================================
// Theme (applied to both views)
// ============================================================

void AutomationWidget::applyDarkTheme()
{
    QString btnBase = R"(
        QPushButton {
            background-color: #FFFFFF; color: #212121;
            border: 1px solid #E0E0E0; border-radius: 4px;
            padding: 4px 16px; font-size: 12px; font-weight: bold;
        }
        QPushButton:hover    { background-color: #E0E0E0; }
        QPushButton:pressed  { background-color: #BDBDBD; }
        QPushButton:disabled { background-color: #F5F7FA; color: #9E9E9E; border-color: #FFFFFF; }
    )";

    QString runBtnStyle = R"(
        QPushButton {
            background-color: #2196F3; color: #FFFFFF;
            border: 1px solid #1976D2; border-radius: 4px;
            padding: 4px 16px; font-size: 12px; font-weight: bold;
        }
        QPushButton:hover    { background-color: #42A5F5; }
        QPushButton:pressed  { background-color: #64B5F6; }
        QPushButton:disabled { background-color: #F5F7FA; color: #9E9E9E; border-color: #FFFFFF; }
    )";

    QString stopBtnStyle = R"(
        QPushButton {
            background-color: #F44336; color: #FFFFFF;
            border: 1px solid #E53935; border-radius: 4px;
            padding: 4px 16px; font-size: 12px; font-weight: bold;
        }
        QPushButton:hover    { background-color: #EF5350; }
        QPushButton:pressed  { background-color: #D32F2F; }
        QPushButton:disabled { background-color: #F5F7FA; color: #9E9E9E; border-color: #FFFFFF; }
    )";

    m_loadBtn->setStyleSheet(btnBase);
    m_runBtn->setStyleSheet(runBtnStyle);
    m_stopBtn->setStyleSheet(stopBtnStyle);
    m_pauseBtn->setStyleSheet(btnBase);
    m_resumeBtn->setStyleSheet(btnBase);

    m_progressBar->setStyleSheet(R"(
        QProgressBar { background-color: #F5F7FA; border: 1px solid #FFFFFF; border-radius: 4px; }
        QProgressBar::chunk { background-color: #2196F3; border-radius: 3px; }
    )");

    // Step table (only relevant in table mode)
    if (m_stepTable) {
        m_stepTable->setStyleSheet(R"(
            QTableWidget {
                background-color: #FFFFFF; color: #212121;
                border: 1px solid #FFFFFF; border-radius: 4px;
                gridline-color: #E0E0E0; font-size: 12px;
            }
            QTableWidget::item { padding: 4px 8px; border-bottom: 1px solid #E0E0E0; }
            QTableWidget::item:selected { background-color: #E3F2FD; color: #2196F3; }
            QHeaderView::section {
                background-color: #F5F7FA; color: #2196F3;
                border: none; border-bottom: 2px solid #E0E0E0;
                padding: 6px 8px; font-weight: bold; font-size: 12px;
            }
        )");
    }

    if (m_detailGroup) {
        m_detailGroup->setStyleSheet(R"(
            QGroupBox {
                background-color: #FFFFFF; color: #212121;
                border: 1px solid #FFFFFF; border-radius: 6px;
                margin-top: 8px; padding-top: 16px;
                font-size: 13px; font-weight: bold;
            }
            QGroupBox::title {
                subcontrol-origin: margin; left: 12px; padding: 0 6px;
                color: #2196F3;
            }
        )");
    }

    m_stepLog->setStyleSheet(R"(
        QPlainTextEdit {
            background-color: #FFFFFF; color: #4CAF50;
            border: 1px solid #FFFFFF; border-radius: 4px;
            font-family: 'Consolas', 'Courier New', monospace;
            font-size: 12px; padding: 4px;
        }
    )");
}

// ============================================================
// View toggle
// ============================================================

void AutomationWidget::onToggleView()
{
    if (!m_viewStack) return;

    int currentIdx = m_viewStack->currentIndex();
    int newIdx = (currentIdx == 0) ? 1 : 0;

    m_viewStack->setCurrentIndex(newIdx);

    // Update toggle button text
    m_toggleViewBtn->setText(newIdx == 0 ? tr("流程图视图") : tr("表格视图"));

    // Update load/run button connections
    disconnect(m_loadBtn, nullptr, this, nullptr);
    disconnect(m_runBtn,  nullptr, this, nullptr);

    if (newIdx == 0) {
        // Table mode
        connect(m_loadBtn, &QPushButton::clicked, this, &AutomationWidget::onLoadTestCase);
        connect(m_runBtn,  &QPushButton::clicked, this, &AutomationWidget::onRun);
        m_loadBtn->setText(tr("加载测试..."));
        m_caseNameLabel->setText(QString::fromStdString(
            m_engine ? m_engine->currentTestCase().name : ""));
        if (m_caseNameLabel->text().isEmpty())
            m_caseNameLabel->setText(tr("未加载测试用例"));
        bool hasCase = m_engine && !m_engine->currentTestCase().steps.empty();
        m_runBtn->setEnabled(hasCase);
    } else {
        // Flowchart mode
        connect(m_loadBtn, &QPushButton::clicked, this, &AutomationWidget::onLoadFlowGraph);
        connect(m_runBtn,  &QPushButton::clicked, this, &AutomationWidget::onRunFlowGraph);
        m_loadBtn->setText(tr("加载流程..."));
        if (!m_currentFlowGraph.name.empty()) {
            m_caseNameLabel->setText(QString::fromStdString(m_currentFlowGraph.name));
        } else {
            m_caseNameLabel->setText(tr("未加载流程图"));
        }
        bool hasGraph = !m_currentFlowGraph.nodes.empty();
        m_runBtn->setEnabled(hasGraph);
    }

    updateButtonStates(false, false);
}

// ============================================================
// Button state management
// ============================================================

void AutomationWidget::updateButtonStates(bool running, bool paused)
{
    bool isFlowchartMode = m_viewStack && m_viewStack->currentIndex() == 1;

    bool hasCase = false;
    if (isFlowchartMode) {
        hasCase = !m_currentFlowGraph.nodes.empty();
    } else {
        hasCase = m_engine && !m_engine->currentTestCase().steps.empty();
    }

    m_loadBtn->setEnabled(!running);
    m_runBtn->setEnabled(hasCase && !running);
    m_pauseBtn->setEnabled(running && !paused);
    m_resumeBtn->setEnabled(running && paused);
    m_stopBtn->setEnabled(running);
    m_toggleViewBtn->setEnabled(!running);
}

// ============================================================
// Step table helpers
// ============================================================

void AutomationWidget::refreshStepTable()
{
    if (!m_stepTable) return;
    m_stepTable->setRowCount(0);

    if (!m_engine) return;

    const auto& tc = m_engine->currentTestCase();
    const int n = static_cast<int>(tc.steps.size());
    m_stepTable->setRowCount(n);

    m_stepStatuses.assign(n, StepRunStatus::Pending);
    m_totalSteps = n;
    m_passedSteps = 0;
    m_failedSteps = 0;
    m_skippedSteps = 0;

    for (int i = 0; i < n; ++i) {
        const auto& step = tc.steps[i];

        // Column 0: step number
        auto* idxItem = new QTableWidgetItem(QString::number(i + 1));
        idxItem->setTextAlignment(Qt::AlignCenter);
        m_stepTable->setItem(i, 0, idxItem);

        // Column 1: type
        auto* typeItem = new QTableWidgetItem(stepTypeLabel(step.type));
        typeItem->setTextAlignment(Qt::AlignCenter);
        m_stepTable->setItem(i, 1, typeItem);

        // Column 2: params summary
        auto* paramsItem = new QTableWidgetItem(paramsSummary(step));
        m_stepTable->setItem(i, 2, paramsItem);

        // Column 3: status
        auto* statusItem = new QTableWidgetItem(QStringLiteral("—"));
        statusItem->setTextAlignment(Qt::AlignCenter);
        m_stepTable->setItem(i, 3, statusItem);

        // Column 4: duration
        auto* durItem = new QTableWidgetItem(QStringLiteral("-"));
        durItem->setTextAlignment(Qt::AlignCenter);
        m_stepTable->setItem(i, 4, durItem);

        // Default row color
        updateRowColor(i, StepRunStatus::Pending);
    }
}

void AutomationWidget::updateRowColor(int row, StepRunStatus status)
{
    if (!m_stepTable || row < 0 || row >= m_stepTable->rowCount()) return;

    QColor bg, fg;
    QString statusText;
    switch (status) {
    case StepRunStatus::Running:
        bg = QColor("#E3F2FD");  // light blue tint
        fg = QColor("#2196F3");
        statusText = QStringLiteral("▶");
        break;
    case StepRunStatus::Passed:
        bg = QColor("#E8F5E9");  // light green tint
        fg = QColor("#4CAF50");
        statusText = QStringLiteral("✓");
        break;
    case StepRunStatus::Failed:
        bg = QColor("#FFEBEE");  // light red tint
        fg = QColor("#F44336");
        statusText = QStringLiteral("✗");
        break;
    case StepRunStatus::Skipped:
        bg = QColor("#FFF8E1");  // light yellow tint
        fg = QColor("#FF9800");
        statusText = QStringLiteral("—");
        break;
    case StepRunStatus::Pending:
    default:
        bg = QColor("#FFFFFF");  // default white
        fg = QColor("#212121");
        statusText = QStringLiteral("—");
        break;
    }

    for (int col = 0; col < m_stepTable->columnCount(); ++col) {
        auto* item = m_stepTable->item(row, col);
        if (item) {
            item->setBackground(bg);
            item->setForeground(fg);
        }
    }

    // Update status column text
    auto* statusItem = m_stepTable->item(row, 3);
    if (statusItem) {
        statusItem->setText(statusText);
    }
}

// ============================================================
// Summary display
// ============================================================

void AutomationWidget::showSummary(const TestResult& result)
{
    // Calculate skipped = total - passed - failed
    m_skippedSteps = m_totalSteps - m_passedSteps - m_failedSteps;
    if (m_skippedSteps < 0) m_skippedSteps = 0;

    QString summary = QString(
        "汇总: 总计:%1 | 通过:%2 | 失败:%3 | 跳过:%4 | 耗时:%5 ms")
        .arg(m_totalSteps)
        .arg(m_passedSteps)
        .arg(m_failedSteps)
        .arg(m_skippedSteps)
        .arg(result.duration.count());

    m_summaryLabel->setText(summary);
    m_summaryLabel->setStyleSheet(
        QString("color: %1; font-size: 13px; font-weight: bold; padding: 4px 0;")
            .arg(result.passed ? "#4CAF50" : "#F44336"));
    m_summaryLabel->setVisible(true);
}

// ============================================================
// Slots — user actions (table mode)
// ============================================================

void AutomationWidget::onLoadTestCase()
{
    QString path = QFileDialog::getOpenFileName(
        this, tr("加载测试用例"), QString(),
        tr("JSON 文件 (*.json);;所有文件 (*)"));

    if (path.isEmpty()) return;

    if (!m_engine) {
        QMessageBox::warning(this, tr("错误"), tr("自动化引擎未初始化"));
        return;
    }

    bool ok = m_engine->loadTestCase(path.toStdString());
    if (!ok) {
        QMessageBox::warning(this, tr("错误"), tr("加载测试用例失败。\n请检查文件格式。"));
        return;
    }

    // Populate the step table
    refreshStepTable();

    const auto& tc = m_engine->currentTestCase();
    m_caseNameLabel->setText(QString::fromStdString(tc.name));

    m_stepLog->clear();
    m_stepLog->appendPlainText(
        QString("[%1] 已加载: %2 (%3 步)")
            .arg(QDateTime::currentDateTime().toString("hh:mm:ss.zzz"),
                 QString::fromStdString(tc.name))
            .arg(tc.steps.size()));

    m_progressBar->setValue(0);
    m_statusLabel->setText(tr("就绪"));
    m_statusLabel->setStyleSheet("color: #4CAF50; font-size: 13px;");
    m_summaryLabel->setVisible(false);

    updateButtonStates(false, false);
}

void AutomationWidget::onRun()
{
    if (!m_engine) return;

    m_stepLog->clear();
    m_stepLog->appendPlainText(
        QString("[%1] 开始测试: %2")
            .arg(QDateTime::currentDateTime().toString("hh:mm:ss.zzz"),
                 QString::fromStdString(m_engine->currentTestCase().name)));

    m_statusLabel->setText(tr("运行中..."));
    m_statusLabel->setStyleSheet("color: #2196F3; font-size: 13px; font-weight: bold;");
    m_progressBar->setValue(0);
    m_summaryLabel->setVisible(false);

    // Reset counters and step statuses
    m_totalSteps = static_cast<int>(m_engine->currentTestCase().steps.size());
    m_passedSteps = 0;
    m_failedSteps = 0;
    m_skippedSteps = 0;
    m_stepStatuses.assign(m_totalSteps, StepRunStatus::Pending);
    for (int i = 0; i < m_totalSteps; ++i) {
        updateRowColor(i, StepRunStatus::Pending);
    }

    updateButtonStates(true, false);

    // Invoke engine->run() on its worker thread (TestRunner's thread)
    QMetaObject::invokeMethod(m_engine, "run", Qt::QueuedConnection);
}

void AutomationWidget::onStop()
{
    if (m_flowRunner) {
        m_flowRunner->stop();
    }
    if (m_engine) {
        m_engine->stop();
    }
    m_statusLabel->setText(tr("停止中..."));
    m_statusLabel->setStyleSheet("color: #F44336; font-size: 13px;");
}

void AutomationWidget::onPause()
{
    if (m_flowRunner) {
        m_flowRunner->pause();
    }
    if (m_engine) {
        m_engine->pause();
    }
    m_statusLabel->setText(tr("已暂停"));
    m_statusLabel->setStyleSheet("color: #FF9800; font-size: 13px; font-weight: bold;");
    updateButtonStates(true, true);
}

void AutomationWidget::onResume()
{
    if (m_engine) {
        m_engine->resume();
    }
    m_statusLabel->setText(tr("运行中..."));
    m_statusLabel->setStyleSheet("color: #2196F3; font-size: 13px; font-weight: bold;");
    updateButtonStates(true, false);
}

// ============================================================
// Step detail panel (on row selection)
// ============================================================

void AutomationWidget::onStepSelected()
{
    if (!m_stepTable) return;
    int row = m_stepTable->currentRow();
    if (row < 0 || !m_engine) {
        if (m_detailTypeLabel) m_detailTypeLabel->setText(QStringLiteral("-"));
        if (m_detailDescLabel) m_detailDescLabel->setText(QStringLiteral("-"));
        if (m_detailTimeoutLabel) m_detailTimeoutLabel->setText(QStringLiteral("-"));
        if (m_detailRetryLabel) m_detailRetryLabel->setText(QStringLiteral("-"));
        return;
    }

    const auto& tc = m_engine->currentTestCase();
    if (row >= static_cast<int>(tc.steps.size())) return;

    const auto& step = tc.steps[row];

    if (m_detailTypeLabel) m_detailTypeLabel->setText(stepTypeLabel(step.type));
    if (m_detailDescLabel) m_detailDescLabel->setText(QString::fromStdString(step.description));
    if (m_detailTimeoutLabel) m_detailTimeoutLabel->setText(QString("%1 ms").arg(step.timeoutMs));
    if (m_detailRetryLabel) m_detailRetryLabel->setText(QString::number(step.retryCount));

    // Remove old dynamic param rows (keep the first 4 fixed rows)
    if (m_detailLayout) {
        while (m_detailLayout->rowCount() > 4) {
            m_detailLayout->removeRow(4);
        }

        // Add param key-value rows
        for (const auto& kv : step.params) {
            auto* keyLabel = new QLabel(QString::fromStdString(kv.first));
            keyLabel->setStyleSheet("color: #C2185B; font-size: 12px;");
            auto* valLabel = new QLabel(QString::fromStdString(kv.second));
            valLabel->setStyleSheet("color: #00796B; font-size: 12px;");
            m_detailLayout->addRow(keyLabel, valLabel);
        }
    }
}

// ============================================================
// Slots — engine signals (table mode)
// ============================================================

void AutomationWidget::onTestStarted(const std::string& caseName)
{
    m_stepLog->appendPlainText(
        QString("[%1] === 测试开始: %2 ===")
            .arg(QDateTime::currentDateTime().toString("hh:mm:ss.zzz"),
                 QString::fromStdString(caseName)));
}

void AutomationWidget::onTestCompleted(const TestResult& result)
{
    QString status = result.passed ? "PASSED" : "FAILED";

    m_stepLog->appendPlainText(
        QString("[%1] === 测试%2 === (耗时: %3 ms)")
            .arg(QDateTime::currentDateTime().toString("hh:mm:ss.zzz"),
                 status)
            .arg(result.duration.count()));

    if (!result.passed) {
        m_stepLog->appendPlainText(
            QString("  错误: %1").arg(QString::fromStdString(result.errorMessage)));
    }

    m_statusLabel->setText(status);
    m_statusLabel->setStyleSheet(
        QString("color: %1; font-size: 13px; font-weight: bold;")
            .arg(result.passed ? "#4CAF50" : "#F44336"));

    showSummary(result);
    updateButtonStates(false, false);
}

void AutomationWidget::onStepStarted(int stepIndex, const std::string& /*description*/)
{
    if (stepIndex < 0 || stepIndex >= static_cast<int>(m_stepStatuses.size())) return;

    // If any prior step is still marked Running (not yet completed), mark it Passed
    for (int i = 0; i < stepIndex; ++i) {
        if (m_stepStatuses[i] == StepRunStatus::Running) {
            m_stepStatuses[i] = StepRunStatus::Passed;
            m_passedSteps++;
            updateRowColor(i, StepRunStatus::Passed);
        }
    }

    m_stepStatuses[stepIndex] = StepRunStatus::Running;
    updateRowColor(stepIndex, StepRunStatus::Running);
    if (m_stepTable) {
        m_stepTable->scrollToItem(m_stepTable->item(stepIndex, 0));
        m_stepTable->selectRow(stepIndex);
    }
}

void AutomationWidget::onStepCompleted(int stepIndex, bool success)
{
    if (stepIndex < 0 || stepIndex >= static_cast<int>(m_stepStatuses.size())) return;

    if (success) {
        m_stepStatuses[stepIndex] = StepRunStatus::Passed;
        m_passedSteps++;
    } else {
        m_stepStatuses[stepIndex] = StepRunStatus::Failed;
        m_failedSteps++;
    }
    updateRowColor(stepIndex, m_stepStatuses[stepIndex]);
}

void AutomationWidget::onProgressUpdated(int current, int total)
{
    if (total > 0) {
        m_progressBar->setRange(0, total);
        m_progressBar->setValue(current);
    }
}

void AutomationWidget::onLogMessage(const std::string& message)
{
    QString ts = QDateTime::currentDateTime().toString("hh:mm:ss.zzz");
    m_stepLog->appendPlainText(QString("[%1] %2").arg(ts, QString::fromStdString(message)));
}

// ============================================================
// Flowchart mode slots (WI-207)
// ============================================================

// --- FlowGraph loading ---

bool AutomationWidget::loadFlowGraph(const QString& jsonPath)
{
    QFile file(jsonPath);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "AutomationWidget: Cannot open flow graph file:" << jsonPath;
        return false;
    }

    QByteArray data = file.readAll();
    file.close();

    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        qWarning() << "AutomationWidget: JSON parse error:" << parseError.errorString();
        return false;
    }

    if (!doc.isObject()) {
        qWarning() << "AutomationWidget: JSON root is not an object";
        return false;
    }

    // Try to parse as FlowGraph first
    auto graphOpt = FlowGraph::fromJson(doc.object());
    if (graphOpt.has_value()) {
        m_currentFlowGraph = std::move(graphOpt.value());
    } else {
        // Try to load as old table-format TestCase and convert
        if (m_engine && m_engine->loadTestCase(jsonPath.toStdString())) {
            const auto& tc = m_engine->currentTestCase();
            m_currentFlowGraph = FlowGraph::fromTestCase(tc);
        } else {
            qWarning() << "AutomationWidget: Failed to parse flow graph or test case";
            return false;
        }
    }

    // Load into canvas
    if (m_flowCanvas) {
        m_flowCanvas->loadGraph(m_currentFlowGraph);
    }

    return true;
}

void AutomationWidget::onLoadFlowGraph()
{
    QString path = QFileDialog::getOpenFileName(
        this, tr("加载流程图"), QString(),
        tr("JSON 文件 (*.json);;所有文件 (*)"));

    if (path.isEmpty()) return;

    bool ok = loadFlowGraph(path);
    if (!ok) {
        QMessageBox::warning(this, tr("错误"), tr("加载流程图失败。\n请检查文件格式。"));
        return;
    }

    m_caseNameLabel->setText(QString::fromStdString(m_currentFlowGraph.name));
    m_stepLog->clear();
    m_stepLog->appendPlainText(
        QString("[%1] 已加载流程图: %2 (%3 节点)")
            .arg(QDateTime::currentDateTime().toString("hh:mm:ss.zzz"),
                 QString::fromStdString(m_currentFlowGraph.name))
            .arg(static_cast<int>(m_currentFlowGraph.nodes.size())));

    m_progressBar->setValue(0);
    m_statusLabel->setText(tr("就绪"));
    m_statusLabel->setStyleSheet("color: #4CAF50; font-size: 13px;");
    m_summaryLabel->setVisible(false);

    updateButtonStates(false, false);
}

// --- Flow graph execution ---

void AutomationWidget::runFlowGraph()
{
    if (m_currentFlowGraph.nodes.empty()) {
        QMessageBox::warning(this, tr("错误"), tr("没有加载流程图。"));
        return;
    }

    if (!m_engine) {
        QMessageBox::warning(this, tr("错误"), tr("自动化引擎未初始化"));
        return;
    }

    m_stepLog->clear();
    m_stepLog->appendPlainText(
        QString("[%1] 开始流程图: %2")
            .arg(QDateTime::currentDateTime().toString("hh:mm:ss.zzz"),
                 QString::fromStdString(m_currentFlowGraph.name)));

    m_statusLabel->setText(tr("运行中..."));
    m_statusLabel->setStyleSheet("color: #2196F3; font-size: 13px; font-weight: bold;");
    m_progressBar->setValue(0);
    m_summaryLabel->setVisible(false);

    // Clear previous highlights
    if (m_flowCanvas) {
        m_flowCanvas->clearAllHighlights();
    }

    updateButtonStates(true, false);

    // Setup FlowRunner with worker thread
    // Create FlowRunner (no parent — we'll manage it)
    if (m_flowRunner) {
        m_flowRunner->stop();
        delete m_flowRunner;
        m_flowRunner = nullptr;
    }

    m_flowRunner = new FlowRunner(m_engine);

    // Connect FlowRunner signals
    connect(m_flowRunner, &FlowRunner::nodeStarted,
            this, &AutomationWidget::onFlowRunnerNodeStarted,
            Qt::QueuedConnection);
    connect(m_flowRunner, &FlowRunner::nodeCompleted,
            this, &AutomationWidget::onFlowRunnerNodeCompleted,
            Qt::QueuedConnection);
    connect(m_flowRunner, &FlowRunner::runnerFinished,
            this, &AutomationWidget::onFlowRunnerFinished,
            Qt::QueuedConnection);
    connect(m_flowRunner, &FlowRunner::logMessage,
            this, &AutomationWidget::onFlowRunnerLogMessage,
            Qt::QueuedConnection);

    // Create worker thread
    auto* workerThread = new QThread(this);
    workerThread->setObjectName("FlowRunnerWorker");

    // Create worker adapter (lives on worker thread)
    auto* worker = new FlowRunWorker();
    worker->runner = m_flowRunner;
    worker->graph = m_currentFlowGraph;

    // Build ExecutionContext
    VariableScope* scope = new VariableScope();
    // Declare graph variables in scope
    for (const auto& varName : m_currentFlowGraph.variables) {
        scope->setNumber(varName, 0.0);
    }

    worker->ctx.variables = scope;
    worker->ctx.engine = m_engine;
    worker->ctx.log = [](const std::string&) {
        // Log messages come through Qt signals (FlowRunner::logMessage)
    };

    // Move worker + runner to worker thread
    m_flowRunner->moveToThread(workerThread);
    worker->moveToThread(workerThread);

    // Clean up thread when done
    connect(workerThread, &QThread::finished, worker, &QObject::deleteLater);
    connect(workerThread, &QThread::finished, scope, &QObject::deleteLater);

    // Invoke run on worker thread
    workerThread->start();
    QMetaObject::invokeMethod(worker, "doRun", Qt::QueuedConnection);
}

void AutomationWidget::onRunFlowGraph()
{
    runFlowGraph();
}

// --- FlowRunner signal handlers ---

void AutomationWidget::onFlowNodeSelected(const std::string& nodeId)
{
    if (!m_paramPanel) return;

    // Find the node in the current flow graph (persistent, mutable)
    FlowNode* foundNode = const_cast<FlowNode*>(m_currentFlowGraph.findNode(nodeId));
    if (foundNode) {
        m_paramPanel->setNode(foundNode);
    } else {
        m_paramPanel->clearNode();
    }
}

void AutomationWidget::onFlowNodeDeselected()
{
    if (m_paramPanel) {
        m_paramPanel->clearNode();
    }
}

void AutomationWidget::onFlowRunnerNodeStarted(const std::string& nodeId)
{
    if (m_flowCanvas) {
        m_flowCanvas->highlightNode(nodeId, true);
    }
    onLogMessage("[流程图] 节点开始: " + nodeId);
}

void AutomationWidget::onFlowRunnerNodeCompleted(const std::string& nodeId, bool success, const std::string& error)
{
    // Keep highlight but change color/intensity (handled by FlowCanvas)
    // For now, just log the result
    QString statusStr = success ? "成功" : "失败";
    QString msg = QString("[流程图] 节点完成: %1 [%2]").arg(QString::fromStdString(nodeId), statusStr);
    if (!error.empty()) {
        msg += " 错误: " + QString::fromStdString(error);
    }
    onLogMessage(msg.toStdString());
}

void AutomationWidget::onFlowRunnerFinished(const FlowRunResult& result)
{
    m_statusLabel->setText(result.passed ? "PASSED" : "FAILED");
    m_statusLabel->setStyleSheet(
        QString("color: %1; font-size: 13px; font-weight: bold;")
            .arg(result.passed ? "#4CAF50" : "#F44336"));

    // Build flow summary
    int totalNodes = static_cast<int>(result.stepResults.size());
    int passedCount = 0;
    int failedCount = 0;
    for (const auto& sr : result.stepResults) {
        if (sr.passed) passedCount++; else failedCount++;
    }

    QString summary = QString("流程图汇总: 总计:%1 | 通过:%2 | 失败:%3 | 耗时:%4 ms")
        .arg(totalNodes)
        .arg(passedCount)
        .arg(failedCount)
        .arg(result.totalDuration.count());

    m_summaryLabel->setText(summary);
    m_summaryLabel->setStyleSheet(
        QString("color: %1; font-size: 13px; font-weight: bold; padding: 4px 0;")
            .arg(result.passed ? "#4CAF50" : "#F44336"));
    m_summaryLabel->setVisible(true);

    m_stepLog->appendPlainText(
        QString("[%1] === 流程图%2 === (耗时: %3 ms)")
            .arg(QDateTime::currentDateTime().toString("hh:mm:ss.zzz"),
                 result.passed ? "PASSED" : "FAILED")
            .arg(result.totalDuration.count()));

    if (!result.errorMessage.empty()) {
        m_stepLog->appendPlainText(
            QString("  错误: %1").arg(QString::fromStdString(result.errorMessage)));
    }

    // Clear highlights
    if (m_flowCanvas) {
        m_flowCanvas->clearAllHighlights();
    }

    // Quit the worker thread now that the run is complete
    if (m_flowRunner) {
        QThread* workerThread = m_flowRunner->thread();
        if (workerThread && workerThread != this->thread()) {
            workerThread->quit();
            workerThread->wait(3000);
        }
    }

    updateButtonStates(false, false);
}

void AutomationWidget::onFlowRunnerLogMessage(const std::string& message)
{
    onLogMessage(message);
}

} // namespace MotorStudio

#include "AutomationWidget.moc"
