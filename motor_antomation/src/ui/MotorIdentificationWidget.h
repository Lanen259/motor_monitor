#pragma once
#include <QWidget>
#include <QPushButton>
#include <QTableWidget>
#include <QTextEdit>
#include <QLabel>
#include <QVector>
#include <QProgressBar>

// 前向声明
namespace MotorStudio {
class CommandBuilder;
}

namespace MotorStudio {

// ============================================================
// 辨识步骤状态
// ============================================================
enum class IdentificationStatus {
    Waiting    = 0,  // 等待中
    Running    = 1,  // 执行中
    Complete   = 2,  // 完成
    Failed     = 3,  // 失败
    Skipped    = 4,  // 跳过
};

// ============================================================
// 单个辨识步骤
// ============================================================
struct IdentificationStep {
    QString              name;       // 步骤名称
    IdentificationStatus status = IdentificationStatus::Waiting;
    double               value  = 0.0;
    QString              unit;       // 单位
    QString              log;        // 步骤日志
    QString              errorMsg;   // 失败时的错误信息
};

// ============================================================
// 电机参数辨识控件
// 提供电机参数辨识的UI界面，支持5个步骤的自动辨识流程
// ============================================================
class MotorIdentificationWidget : public QWidget {
    Q_OBJECT
public:
    explicit MotorIdentificationWidget(QWidget* parent = nullptr);
    ~MotorIdentificationWidget() override;

    // 设置 CommandBuilder（用于构建辨识命令）
    void setCommandBuilder(CommandBuilder* builder);

    // 获取辨识结果
    QVector<IdentificationStep> results() const;

    // 是否正在辨识中
    bool isRunning() const;

    // 重置所有步骤
    void reset();

signals:
    // 辨识开始
    void identificationStarted();

    // 辨识被用户停止
    void identificationStopped();

    // 辨识完成（所有步骤完成或失败）
    void identificationComplete();

    // 请求发送命令到MCU
    void commandRequested(const QByteArray& cmd);

    // 单个步骤完成
    void stepCompleted(int stepIndex, IdentificationStatus status);

private slots:
    void onStartIdentification();
    void onStopIdentification();
    void onExportResults();

private:
    void setupUI();
    void applyDarkTheme();
    void updateUI();
    void executeNextStep();
    void appendLog(const QString& message);
    void setStepStatus(int index, IdentificationStatus status, double value = 0.0);
    void updateResultTable();
    QString statusToString(IdentificationStatus status) const;
    QString statusToIcon(IdentificationStatus status) const;
    QColor  statusToColor(IdentificationStatus status) const;
    void exportToJson(const QString& path);

    // === 操作按钮 ===
    QPushButton* m_startBtn;
    QPushButton* m_stopBtn;
    QPushButton* m_exportBtn;

    // === 步骤显示区域 ===
    QVector<QLabel*>      m_stepNameLabels;
    QVector<QLabel*>      m_stepStatusLabels;
    QVector<QLabel*>      m_stepValueLabels;
    QVector<QProgressBar*> m_stepProgressBars;
    QVector<QFrame*>      m_stepFrames;

    // === 日志区域 ===
    QTextEdit* m_logView;

    // === 结果表格 ===
    QTableWidget* m_resultTable;

    // === 状态标签 ===
    QLabel* m_statusLabel;

    // === 数据 ===
    QVector<IdentificationStep> m_steps;
    int m_currentStepIndex = -1;
    bool m_running = false;

    // === CommandBuilder ===
    CommandBuilder* m_cmdBuilder = nullptr;
};

} // namespace MotorStudio