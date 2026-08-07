#pragma once
#include <QWidget>
#include <QTableWidget>
#include <QPushButton>
#include <QProgressBar>
#include <QPlainTextEdit>
#include <QLabel>
#include <QGroupBox>
#include <QFormLayout>
#include <QSplitter>
#include <QStackedWidget>
#include <memory>
#include <vector>
#include <string>
#include "automation/FlowGraph.h"

namespace MotorStudio {

class AutomationEngine;
struct TestCase;
struct TestStep;
struct TestResult;
enum class StepType : uint8_t;

class FlowCanvas;
class NodeLibraryPanel;
class NodeParamPanel;
class FlowRunner;
struct FlowRunResult;

// Status per step for coloring the table rows
enum class StepRunStatus { Pending, Running, Passed, Failed, Skipped };

// Automation test panel — WI-013 (table mode) + WI-207 (flowchart mode)
class AutomationWidget : public QWidget {
    Q_OBJECT
public:
    explicit AutomationWidget(AutomationEngine* engine, QWidget* parent = nullptr);
    ~AutomationWidget() override;

    void connectEngine(AutomationEngine* engine);

    // --- FlowGraph integration (WI-207) ---
    bool loadFlowGraph(const QString& jsonPath);
    void runFlowGraph();

private slots:
    // --- Table mode slots ---
    void onLoadTestCase();
    void onRun();
    void onStop();
    void onPause();
    void onResume();

    void onTestStarted(const std::string& caseName);
    void onTestCompleted(const TestResult& result);
    void onStepStarted(int stepIndex, const std::string& description);
    void onStepCompleted(int stepIndex, bool success);
    void onProgressUpdated(int current, int total);
    void onLogMessage(const std::string& message);

    void onStepSelected();

    // --- Flowchart mode slots (WI-207) ---
    void onLoadFlowGraph();
    void onRunFlowGraph();
    void onToggleView();
    void onFlowNodeSelected(const std::string& nodeId);
    void onFlowNodeDeselected();
    void onFlowRunnerNodeStarted(const std::string& nodeId);
    void onFlowRunnerNodeCompleted(const std::string& nodeId, bool success, const std::string& error);
    void onFlowRunnerFinished(const FlowRunResult& result);
    void onFlowRunnerLogMessage(const std::string& message);

private:
    void setupUi();
    void setupTableUi(QWidget* page);   // existing table-based UI (extracted)
    void setupFlowUi(QWidget* page);    // new flowchart IDE layout (WI-207)
    void updateButtonStates(bool running, bool paused);
    void applyDarkTheme();
    void refreshStepTable();
    void updateRowColor(int row, StepRunStatus status);
    void showSummary(const TestResult& result);

    // Shared
    AutomationEngine* m_engine = nullptr;

    // Current step status tracking
    std::vector<StepRunStatus> m_stepStatuses;

    // Summary counters
    int m_totalSteps = 0;
    int m_passedSteps = 0;
    int m_failedSteps = 0;
    int m_skippedSteps = 0;

    // UI elements — toolbar
    QPushButton* m_loadBtn     = nullptr;
    QPushButton* m_runBtn      = nullptr;
    QPushButton* m_pauseBtn    = nullptr;
    QPushButton* m_resumeBtn   = nullptr;
    QPushButton* m_stopBtn     = nullptr;

    QLabel*         m_caseNameLabel = nullptr;
    QLabel*         m_statusLabel   = nullptr;
    QProgressBar*   m_progressBar   = nullptr;

    // View toggle
    QPushButton*    m_toggleViewBtn = nullptr;
    QStackedWidget* m_viewStack     = nullptr;

    // Table-mode specific
    QTableWidget*   m_stepTable     = nullptr;

    // Step detail panel (right area) — table mode
    QGroupBox*      m_detailGroup   = nullptr;
    QFormLayout*    m_detailLayout  = nullptr;
    QLabel*         m_detailTypeLabel   = nullptr;
    QLabel*         m_detailDescLabel   = nullptr;
    QLabel*         m_detailTimeoutLabel = nullptr;
    QLabel*         m_detailRetryLabel  = nullptr;

    // Execution log (bottom) — shared between both modes
    QPlainTextEdit* m_stepLog       = nullptr;

    // Summary label — shared
    QLabel*         m_summaryLabel  = nullptr;

    // --- Flowchart mode (WI-207) ---
    FlowCanvas*         m_flowCanvas    = nullptr;
    NodeLibraryPanel*   m_nodeLibrary   = nullptr;
    NodeParamPanel*     m_paramPanel    = nullptr;
    FlowRunner*         m_flowRunner    = nullptr;
    FlowGraph           m_currentFlowGraph;
};

} // namespace MotorStudio
