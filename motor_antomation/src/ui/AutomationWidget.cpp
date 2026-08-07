#include "AutomationWidget.h"
#include "automation/AutomationEngine.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSplitter>
#include <QHeaderView>
#include <QFileDialog>
#include <QMessageBox>
#include <QDateTime>
#include <QDebug>
#include <sstream>

namespace MotorStudio {

// ============================================================
// Helper: StepType to display string
// ============================================================
static QString stepTypeLabel(StepType t)
{
    switch (t) {
    case StepType::SetParameter: return QStringLiteral("SET");
    case StepType::Wait:         return QStringLiteral("WAIT");
    case StepType::ReadParameter: return QStringLiteral("READ");
    case StepType::Assert:       return QStringLiteral("ASRT");
    case StepType::RecordData:   return QStringLiteral("REC");
    case StepType::StartMotor:   return QStringLiteral("START");
    case StepType::StopMotor:    return QStringLiteral("STOP");
    case StepType::Custom:       return QStringLiteral("CUST");
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
}

AutomationWidget::~AutomationWidget() = default;

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
// UI construction
// ============================================================

void AutomationWidget::setupUi()
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(12, 12, 12, 12);
    mainLayout->setSpacing(8);

    // ---- Row 0: toolbar (case name + status + buttons) ----
    auto* topBar = new QHBoxLayout();
    topBar->setSpacing(8);

    m_caseNameLabel = new QLabel(tr("No test case loaded"));
    m_caseNameLabel->setStyleSheet("color: #2196F3; font-size: 14px; font-weight: bold;");
    topBar->addWidget(m_caseNameLabel);

    m_statusLabel = new QLabel(tr("Idle"));
    m_statusLabel->setStyleSheet("color: #757575; font-size: 13px;");
    topBar->addWidget(m_statusLabel);

    topBar->addStretch();

    m_loadBtn = new QPushButton(tr("Load Test..."));
    m_loadBtn->setFixedHeight(30);
    topBar->addWidget(m_loadBtn);

    m_runBtn = new QPushButton(tr("Run"));
    m_runBtn->setFixedHeight(30);
    m_runBtn->setEnabled(false);
    topBar->addWidget(m_runBtn);

    m_pauseBtn = new QPushButton(tr("Pause"));
    m_pauseBtn->setFixedHeight(30);
    m_pauseBtn->setEnabled(false);
    topBar->addWidget(m_pauseBtn);

    m_resumeBtn = new QPushButton(tr("Resume"));
    m_resumeBtn->setFixedHeight(30);
    m_resumeBtn->setEnabled(false);
    topBar->addWidget(m_resumeBtn);

    m_stopBtn = new QPushButton(tr("Stop"));
    m_stopBtn->setFixedHeight(30);
    m_stopBtn->setEnabled(false);
    topBar->addWidget(m_stopBtn);

    mainLayout->addLayout(topBar);

    // ---- Row 1: Progress bar ----
    m_progressBar = new QProgressBar();
    m_progressBar->setFixedHeight(8);
    m_progressBar->setTextVisible(false);
    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(0);
    mainLayout->addWidget(m_progressBar);

    // ---- Row 2: QSplitter — step table (left) + detail panel (right) ----
    auto* splitter = new QSplitter(Qt::Horizontal);

    // — Left: QTableWidget —
    m_stepTable = new QTableWidget(0, 5); // #, Type, Params, Status, Duration
    m_stepTable->setHorizontalHeaderLabels({tr("#"), tr("Type"), tr("Params"), tr("Status"), tr("Duration")});
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
    m_detailGroup = new QGroupBox(tr("Step Detail"));
    m_detailLayout = new QFormLayout(m_detailGroup);
    m_detailLayout->setContentsMargins(12, 16, 12, 12);
    m_detailLayout->setSpacing(8);

    m_detailTypeLabel    = new QLabel(QStringLiteral("-"));
    m_detailDescLabel    = new QLabel(QStringLiteral("-"));
    m_detailTimeoutLabel = new QLabel(QStringLiteral("-"));
    m_detailRetryLabel   = new QLabel(QStringLiteral("-"));

    auto* detailTypeLbl    = new QLabel(tr("Type:"));
    auto* detailDescLbl    = new QLabel(tr("Description:"));
    auto* detailTimeoutLbl = new QLabel(tr("Timeout:"));
    auto* detailRetryLbl   = new QLabel(tr("Retry Count:"));

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
    mainLayout->addWidget(splitter, 1); // stretch=1

    // ---- Row 3: Execution log ----
    auto* logTitle = new QLabel(tr("Execution Log"));
    logTitle->setStyleSheet("color: #2196F3; font-size: 12px; font-weight: bold;");
    mainLayout->addWidget(logTitle);

    m_stepLog = new QPlainTextEdit();
    m_stepLog->setReadOnly(true);
    m_stepLog->setMaximumBlockCount(2000);
    m_stepLog->setFixedHeight(150);
    mainLayout->addWidget(m_stepLog);

    // ---- Row 4: Summary label ----
    m_summaryLabel = new QLabel();
    m_summaryLabel->setStyleSheet("color: #757575; font-size: 12px; padding: 4px 0;");
    m_summaryLabel->setVisible(false);
    mainLayout->addWidget(m_summaryLabel);

    // ---- Connections ----
    connect(m_loadBtn,  &QPushButton::clicked, this, &AutomationWidget::onLoadTestCase);
    connect(m_runBtn,   &QPushButton::clicked, this, &AutomationWidget::onRun);
    connect(m_stopBtn,  &QPushButton::clicked, this, &AutomationWidget::onStop);
    connect(m_pauseBtn, &QPushButton::clicked, this, &AutomationWidget::onPause);
    connect(m_resumeBtn,&QPushButton::clicked, this, &AutomationWidget::onResume);

    connect(m_stepTable, &QTableWidget::itemSelectionChanged,
            this, &AutomationWidget::onStepSelected);
}

// ============================================================
// Dark theme
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
// Button state management
// ============================================================

void AutomationWidget::updateButtonStates(bool running, bool paused)
{
    bool hasCase = m_engine && !m_engine->currentTestCase().steps.empty();

    m_loadBtn->setEnabled(!running);
    m_runBtn->setEnabled(hasCase && !running);
    m_pauseBtn->setEnabled(running && !paused);
    m_resumeBtn->setEnabled(running && paused);
    m_stopBtn->setEnabled(running);
}

// ============================================================
// Step table helpers
// ============================================================

void AutomationWidget::refreshStepTable()
{
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
        auto* statusItem = new QTableWidgetItem(QStringLiteral("—")); // em dash
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
    if (row < 0 || row >= m_stepTable->rowCount()) return;

    QColor bg, fg;
    QString statusText;
    switch (status) {
    case StepRunStatus::Running:
        bg = QColor("#E3F2FD");  // light blue tint
        fg = QColor("#2196F3");
        statusText = QStringLiteral("▶"); // play triangle
        break;
    case StepRunStatus::Passed:
        bg = QColor("#E8F5E9");  // light green tint
        fg = QColor("#4CAF50");
        statusText = QStringLiteral("✓"); // checkmark
        break;
    case StepRunStatus::Failed:
        bg = QColor("#FFEBEE");  // light red tint
        fg = QColor("#F44336");
        statusText = QStringLiteral("✗"); // cross
        break;
    case StepRunStatus::Skipped:
        bg = QColor("#FFF8E1");  // light yellow tint
        fg = QColor("#FF9800");
        statusText = QStringLiteral("—"); // em dash
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
        "Summary:  Total: %1  |  Passed: %2  |  Failed: %3  |  Skipped: %4  |  Duration: %5 ms")
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
// Slots — user actions
// ============================================================

void AutomationWidget::onLoadTestCase()
{
    QString path = QFileDialog::getOpenFileName(
        this, tr("Load Test Case"), QString(),
        tr("JSON Files (*.json);;All Files (*)"));

    if (path.isEmpty()) return;

    if (!m_engine) {
        QMessageBox::warning(this, tr("Error"), tr("Automation engine not initialized"));
        return;
    }

    bool ok = m_engine->loadTestCase(path.toStdString());
    if (!ok) {
        QMessageBox::warning(this, tr("Error"), tr("Failed to load test case.\nCheck file format."));
        return;
    }

    // Populate the step table
    refreshStepTable();

    const auto& tc = m_engine->currentTestCase();
    m_caseNameLabel->setText(QString::fromStdString(tc.name));

    m_stepLog->clear();
    m_stepLog->appendPlainText(
        QString("[%1] Loaded: %2 (%3 steps)")
            .arg(QDateTime::currentDateTime().toString("hh:mm:ss.zzz"),
                 QString::fromStdString(tc.name))
            .arg(tc.steps.size()));

    m_progressBar->setValue(0);
    m_statusLabel->setText(tr("Ready"));
    m_statusLabel->setStyleSheet("color: #4CAF50; font-size: 13px;");
    m_summaryLabel->setVisible(false);

    updateButtonStates(false, false);
}

void AutomationWidget::onRun()
{
    if (!m_engine) return;

    m_stepLog->clear();
    m_stepLog->appendPlainText(
        QString("[%1] Starting test: %2")
            .arg(QDateTime::currentDateTime().toString("hh:mm:ss.zzz"),
                 QString::fromStdString(m_engine->currentTestCase().name)));

    m_statusLabel->setText(tr("Running..."));
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
    if (!m_engine) return;
    m_engine->stop();
    m_statusLabel->setText(tr("Stopping..."));
    m_statusLabel->setStyleSheet("color: #F44336; font-size: 13px;");
}

void AutomationWidget::onPause()
{
    if (!m_engine) return;
    m_engine->pause();
    m_statusLabel->setText(tr("Paused"));
    m_statusLabel->setStyleSheet("color: #FF9800; font-size: 13px; font-weight: bold;");
    updateButtonStates(true, true);
}

void AutomationWidget::onResume()
{
    if (!m_engine) return;
    m_engine->resume();
    m_statusLabel->setText(tr("Running..."));
    m_statusLabel->setStyleSheet("color: #2196F3; font-size: 13px; font-weight: bold;");
    updateButtonStates(true, false);
}

// ============================================================
// Step detail panel (on row selection)
// ============================================================

void AutomationWidget::onStepSelected()
{
    int row = m_stepTable->currentRow();
    if (row < 0 || !m_engine) {
        m_detailTypeLabel->setText(QStringLiteral("-"));
        m_detailDescLabel->setText(QStringLiteral("-"));
        m_detailTimeoutLabel->setText(QStringLiteral("-"));
        m_detailRetryLabel->setText(QStringLiteral("-"));
        return;
    }

    const auto& tc = m_engine->currentTestCase();
    if (row >= static_cast<int>(tc.steps.size())) return;

    const auto& step = tc.steps[row];

    m_detailTypeLabel->setText(stepTypeLabel(step.type));
    m_detailDescLabel->setText(QString::fromStdString(step.description));
    m_detailTimeoutLabel->setText(QString("%1 ms").arg(step.timeoutMs));
    m_detailRetryLabel->setText(QString::number(step.retryCount));

    // Remove old dynamic param rows (keep the first 4 fixed rows)
    while (m_detailLayout->rowCount() > 4) {
        // QFormLayout::removeRow takes the row index; we keep removing from row 4 onwards
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

// ============================================================
// Slots — engine signals
// ============================================================

void AutomationWidget::onTestStarted(const std::string& caseName)
{
    m_stepLog->appendPlainText(
        QString("[%1] === Test Started: %2 ===")
            .arg(QDateTime::currentDateTime().toString("hh:mm:ss.zzz"),
                 QString::fromStdString(caseName)));
}

void AutomationWidget::onTestCompleted(const TestResult& result)
{
    QString status = result.passed ? "PASSED" : "FAILED";

    m_stepLog->appendPlainText(
        QString("[%1] === Test %2 === (duration: %3 ms)")
            .arg(QDateTime::currentDateTime().toString("hh:mm:ss.zzz"),
                 status)
            .arg(result.duration.count()));

    if (!result.passed) {
        m_stepLog->appendPlainText(
            QString("  Error: %1").arg(QString::fromStdString(result.errorMessage)));
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
    // (handles the case where the completed signal arrived but we missed updating)
    for (int i = 0; i < stepIndex; ++i) {
        if (m_stepStatuses[i] == StepRunStatus::Running) {
            m_stepStatuses[i] = StepRunStatus::Passed;
            m_passedSteps++;
            updateRowColor(i, StepRunStatus::Passed);
        }
    }

    m_stepStatuses[stepIndex] = StepRunStatus::Running;
    updateRowColor(stepIndex, StepRunStatus::Running);
    m_stepTable->scrollToItem(m_stepTable->item(stepIndex, 0));
    m_stepTable->selectRow(stepIndex);
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

} // namespace MotorStudio
