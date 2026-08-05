#pragma once

#include <QWidget>
#include <QPushButton>
#include <QTableWidget>
#include <QTextEdit>
#include <QLabel>
#include <QProgressBar>
#include <QVector>
#include <QByteArray>

// ============================================================
// 电机参数辨识控件
// 5步辨识流程：Rs → Ld/Lq → 磁链 → 编码器偏移 → Hall角度
// ============================================================

// 辨识步骤状态
enum class IdentificationStepStatus {
    Waiting,      // 等待中
    Running,      // 执行中
    Complete,     // 完成
    Failed        // 失败
};

// 单个辨识步骤
struct IdentificationStep {
    QString name;
    QString description;
    IdentificationStepStatus status = IdentificationStepStatus::Waiting;
    double value = 0.0;
    QString unit;
    QString log;
};

class MotorIdentificationWidget : public QWidget {
    Q_OBJECT
public:
    explicit MotorIdentificationWidget(QWidget* parent = nullptr);
    ~MotorIdentificationWidget() override;

    bool isRunning() const { return m_running; }

    // 接收MCU响应数据
    void handleResponse(uint8_t cmd, const QByteArray& data);

    // 取消辨识
    void cancelIdentification();

signals:
    void identificationStarted();
    void identificationStopped();
    void identificationComplete();
    void commandRequested(const QByteArray& cmd);

private slots:
    void onStartIdentification();
    void onStopIdentification();
    void onExportResults();

private:
    void setupUI();
    void setupSteps();
    void applyDarkTheme();
    void updateUI();
    void executeNextStep();
    void appendLog(const QString& text);
    void updateStepStatus(int stepIndex, IdentificationStepStatus st, double val = 0.0);

    // 控件
    QPushButton* m_startBtn;
    QPushButton* m_stopBtn;
    QPushButton* m_exportBtn;
    QTableWidget* m_resultTable;
    QTextEdit* m_logView;
    QLabel* m_statusLabel;
    QProgressBar* m_progressBar;

    // 步骤
    QVector<IdentificationStep> m_steps;
    int m_currentStep;
    bool m_running;
    int m_totalSteps;
};