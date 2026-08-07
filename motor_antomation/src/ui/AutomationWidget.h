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
#include <memory>
#include <vector>
#include <string>

namespace MotorStudio {

class AutomationEngine;
struct TestCase;
struct TestStep;
struct TestResult;
enum class StepType : uint8_t;

// Status per step for coloring the table rows
enum class StepRunStatus { Pending, Running, Passed, Failed, Skipped };

// Automation test panel — WI-013
class AutomationWidget : public QWidget {
    Q_OBJECT
public:
    explicit AutomationWidget(AutomationEngine* engine, QWidget* parent = nullptr);
    ~AutomationWidget() override;

    void connectEngine(AutomationEngine* engine);

private slots:
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

private:
    void setupUi();
    void updateButtonStates(bool running, bool paused);
    void applyDarkTheme();
    void refreshStepTable();
    void updateRowColor(int row, StepRunStatus status);
    void showSummary(const TestResult& result);

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

    // Step table (left area)
    QTableWidget*   m_stepTable     = nullptr;

    // Step detail panel (right area)
    QGroupBox*      m_detailGroup   = nullptr;
    QFormLayout*    m_detailLayout  = nullptr;
    QLabel*         m_detailTypeLabel   = nullptr;
    QLabel*         m_detailDescLabel   = nullptr;
    QLabel*         m_detailTimeoutLabel = nullptr;
    QLabel*         m_detailRetryLabel  = nullptr;
    // Dynamic param labels added in onStepSelected

    // Execution log (bottom)
    QPlainTextEdit* m_stepLog       = nullptr;

    // Summary label
    QLabel*         m_summaryLabel  = nullptr;
};

} // namespace MotorStudio
