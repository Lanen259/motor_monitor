#include "MotorIdentificationWidget.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QFrame>
#include <QHeaderView>
#include <QFileDialog>
#include <QMessageBox>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFile>
#include <QDateTime>
#include <QTimer>
#include <QScrollBar>

MotorIdentificationWidget::MotorIdentificationWidget(QWidget* parent)
    : QWidget(parent)
    , m_currentStep(0)
    , m_running(false)
    , m_totalSteps(0)
{
    setupUI();
    setupSteps();
    applyDarkTheme();
    updateUI();
}

MotorIdentificationWidget::~MotorIdentificationWidget() = default;

void MotorIdentificationWidget::setupUI()
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(12, 12, 12, 12);
    mainLayout->setSpacing(12);

    // ========================================
    // 顶部：状态栏 + 控制按钮
    // ========================================
    auto* topBar = new QHBoxLayout();

    m_statusLabel = new QLabel(tr("就绪 — 等待开始辨识"));
    m_statusLabel->setStyleSheet("QLabel { font-size: 14px; font-weight: bold; }");
    topBar->addWidget(m_statusLabel);

    topBar->addStretch();

    m_startBtn = new QPushButton(tr("▶ 开始辨识"));
    m_startBtn->setMinimumSize(120, 36);
    m_startBtn->setStyleSheet(
        "QPushButton { font-size: 14px; font-weight: bold; background-color: #2e7d32; "
        "color: white; border: none; border-radius: 4px; padding: 6px 16px; }"
        "QPushButton:hover { background-color: #388e3c; }"
        "QPushButton:disabled { background-color: #555; color: #888; }");
    connect(m_startBtn, &QPushButton::clicked, this, &MotorIdentificationWidget::onStartIdentification);
    topBar->addWidget(m_startBtn);

    m_stopBtn = new QPushButton(tr("■ 停止"));
    m_stopBtn->setMinimumSize(100, 36);
    m_stopBtn->setEnabled(false);
    m_stopBtn->setStyleSheet(
        "QPushButton { font-size: 14px; font-weight: bold; background-color: #c62828; "
        "color: white; border: none; border-radius: 4px; padding: 6px 16px; }"
        "QPushButton:hover { background-color: #d32f2f; }"
        "QPushButton:disabled { background-color: #555; color: #888; }");
    connect(m_stopBtn, &QPushButton::clicked, this, &MotorIdentificationWidget::onStopIdentification);
    topBar->addWidget(m_stopBtn);

    m_exportBtn = new QPushButton(tr("导出结果 JSON"));
    m_exportBtn->setMinimumSize(120, 36);
    m_exportBtn->setStyleSheet(
        "QPushButton { font-size: 13px; background-color: #1565c0; "
        "color: white; border: none; border-radius: 4px; padding: 6px 16px; }"
        "QPushButton:hover { background-color: #1976d2; }");
    connect(m_exportBtn, &QPushButton::clicked, this, &MotorIdentificationWidget::onExportResults);
    topBar->addWidget(m_exportBtn);

    mainLayout->addLayout(topBar);

    // ========================================
    // 进度条
    // ========================================
    m_progressBar = new QProgressBar();
    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(0);
    m_progressBar->setTextVisible(true);
    m_progressBar->setFixedHeight(22);
    m_progressBar->setStyleSheet(
        "QProgressBar { border: 1px solid #444; border-radius: 4px; background: #1e1e1e; "
        "text-align: center; color: white; font-size: 12px; }"
        "QProgressBar::chunk { background: #2e7d32; border-radius: 3px; }");
    mainLayout->addWidget(m_progressBar);

    // ========================================
    // 中间：结果表格 + 辨识步骤
    // ========================================
    auto* midLayout = new QHBoxLayout();
    midLayout->setSpacing(12);

    // 结果表格
    auto* tableGroup = new QGroupBox(tr("辨识结果"));
    auto* tableLayout = new QVBoxLayout(tableGroup);
    m_resultTable = new QTableWidget(0, 4);
    m_resultTable->setHorizontalHeaderLabels({tr("参数"), tr("值"), tr("单位"), tr("状态")});
    m_resultTable->horizontalHeader()->setStretchLastSection(true);
    m_resultTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_resultTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_resultTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_resultTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    m_resultTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_resultTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_resultTable->setAlternatingRowColors(true);
    m_resultTable->verticalHeader()->setVisible(false);
    tableLayout->addWidget(m_resultTable);
    midLayout->addWidget(tableGroup, 3);

    // 日志区域
    auto* logGroup = new QGroupBox(tr("辨识日志"));
    auto* logLayout = new QVBoxLayout(logGroup);
    m_logView = new QTextEdit();
    m_logView->setReadOnly(true);
    m_logView->setMinimumWidth(300);
    m_logView->setStyleSheet(
        "QTextEdit { background: #121212; color: #c0c0c0; font-family: Consolas, monospace; "
        "font-size: 12px; border: 1px solid #333; border-radius: 4px; }");
    logLayout->addWidget(m_logView);
    midLayout->addWidget(logGroup, 2);

    mainLayout->addLayout(midLayout, 1);
}

void MotorIdentificationWidget::setupSteps()
{
    m_steps = {
        {tr("相电阻 Rs"),   tr("注入直流电压，测量绕组电阻"), IdentificationStepStatus::Waiting, 0, QString::fromUtf8("Ω")},
        {tr("电感 Ld/Lq"),   tr("注入高频信号，辨识d/q轴电感"), IdentificationStepStatus::Waiting, 0, "mH"},
        {tr("磁链 ψf"),     tr("反电动势法测量永磁体磁链"), IdentificationStepStatus::Waiting, 0, "Wb"},
        {tr("编码器偏移"),   tr("对零操作，校准编码器零位"), IdentificationStepStatus::Waiting, 0, QString::fromUtf8("°")},
        {tr("Hall 角度"),    tr("测量Hall传感器安装角度"), IdentificationStepStatus::Waiting, 0, QString::fromUtf8("°")},
    };
    m_totalSteps = m_steps.size();
}

void MotorIdentificationWidget::applyDarkTheme()
{
    setStyleSheet(
        "QWidget { background: #1e1e1e; color: #e0e0e0; }"
        "QGroupBox { font-weight: bold; border: 1px solid #333; border-radius: 6px; "
        "margin-top: 12px; padding-top: 16px; background: #252525; }"
        "QGroupBox::title { subcontrol-origin: margin; left: 12px; padding: 0 6px; color: #90caf9; }"
        "QTableWidget { background: #1a1a1a; gridline-color: #333; border: 1px solid #333; "
        "border-radius: 4px; font-size: 13px; }"
        "QTableWidget::item { padding: 6px; }"
        "QTableWidget::item:selected { background: #0d47a1; }"
        "QHeaderView::section { background: #2a2a2a; color: #aaa; border: 1px solid #333; "
        "padding: 6px; font-size: 12px; font-weight: bold; }"
        "QTableWidget::item:alternate { background: #222; }"
        "QScrollBar:vertical { background: #1a1a1a; width: 10px; border-radius: 5px; }"
        "QScrollBar::handle:vertical { background: #444; border-radius: 5px; min-height: 30px; }"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }");
}

void MotorIdentificationWidget::updateUI()
{
    m_startBtn->setEnabled(!m_running);
    m_stopBtn->setEnabled(m_running);
    m_exportBtn->setEnabled(!m_running);

    // 更新结果表格
    m_resultTable->setRowCount(m_steps.size());
    for (int i = 0; i < m_steps.size(); ++i) {
        const auto& step = m_steps[i];

        QTableWidgetItem* nameItem = m_resultTable->item(i, 0);
        if (!nameItem) {
            nameItem = new QTableWidgetItem();
            m_resultTable->setItem(i, 0, nameItem);
        }
        nameItem->setText(step.name);

        QTableWidgetItem* valueItem = m_resultTable->item(i, 1);
        if (!valueItem) {
            valueItem = new QTableWidgetItem();
            m_resultTable->setItem(i, 1, valueItem);
        }
        if (step.status == IdentificationStepStatus::Complete) {
            valueItem->setText(QString::number(step.value, 'f', 4));
        } else {
            valueItem->setText("—");
        }

        QTableWidgetItem* unitItem = m_resultTable->item(i, 2);
        if (!unitItem) {
            unitItem = new QTableWidgetItem();
            m_resultTable->setItem(i, 2, unitItem);
        }
        unitItem->setText(step.unit);

        QTableWidgetItem* statusItem = m_resultTable->item(i, 3);
        if (!statusItem) {
            statusItem = new QTableWidgetItem();
            m_resultTable->setItem(i, 3, statusItem);
        }
        switch (step.status) {
        case IdentificationStepStatus::Waiting:
            statusItem->setText(tr("等待"));
            statusItem->setForeground(QColor("#888"));
            break;
        case IdentificationStepStatus::Running:
            statusItem->setText(tr("执行中…"));
            statusItem->setForeground(QColor("#ffa726"));
            break;
        case IdentificationStepStatus::Complete:
            statusItem->setText(tr("✓ 完成"));
            statusItem->setForeground(QColor("#66bb6a"));
            break;
        case IdentificationStepStatus::Failed:
            statusItem->setText(tr("✗ 失败"));
            statusItem->setForeground(QColor("#ef5350"));
            break;
        }
    }
}

void MotorIdentificationWidget::onStartIdentification()
{
    if (m_running) return;

    m_running = true;
    m_currentStep = 0;

    // 重置所有步骤
    for (auto& step : m_steps) {
        step.status = IdentificationStepStatus::Waiting;
        step.value = 0.0;
    }

    m_logView->clear();
    m_progressBar->setValue(0);
    updateUI();

    appendLog(QStringLiteral("=== 电机参数辨识开始 ==="));
    appendLog(QStringLiteral("共 %1 个步骤，按顺序执行").arg(m_totalSteps));

    emit identificationStarted();
    executeNextStep();
}

void MotorIdentificationWidget::onStopIdentification()
{
    if (!m_running) return;

    m_running = false;
    updateUI();

    appendLog(QStringLiteral("⚠ 辨识被用户取消"));

    m_progressBar->setValue(0);
    m_statusLabel->setText(tr("已停止"));
    m_statusLabel->setStyleSheet("QLabel { font-size: 14px; font-weight: bold; color: #ffa726; }");

    emit identificationStopped();
}

void MotorIdentificationWidget::cancelIdentification()
{
    onStopIdentification();
}

void MotorIdentificationWidget::onExportResults()
{
    QString path = QFileDialog::getSaveFileName(
        this, tr("导出辨识结果"), QString(), tr("JSON 文件 (*.json)"));
    if (path.isEmpty()) return;

    QJsonObject root;
    root["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    root["type"] = QStringLiteral("motor_identification");

    QJsonArray stepsArr;
    for (const auto& step : m_steps) {
        QJsonObject obj;
        obj["name"] = step.name;
        obj["description"] = step.description;
        obj["status"] = (step.status == IdentificationStepStatus::Complete) ? "complete"
                        : (step.status == IdentificationStepStatus::Failed) ? "failed"
                        : "pending";
        obj["value"] = step.value;
        obj["unit"] = step.unit;
        stepsArr.append(obj);
    }
    root["steps"] = stepsArr;

    QFile file(path);
    if (file.open(QIODevice::WriteOnly)) {
        QJsonDocument doc(root);
        file.write(doc.toJson(QJsonDocument::Indented));
        file.close();
        appendLog(QStringLiteral("✓ 辨识结果已导出: %1").arg(path));
    } else {
        QMessageBox::warning(this, tr("导出失败"), tr("无法写入文件: %1").arg(path));
    }
}

void MotorIdentificationWidget::executeNextStep()
{
    if (!m_running) return;

    if (m_currentStep >= m_steps.size()) {
        // 全部完成
        m_running = false;
        m_progressBar->setValue(100);
        m_statusLabel->setText(tr("辨识完成"));
        m_statusLabel->setStyleSheet("QLabel { font-size: 14px; font-weight: bold; color: #66bb6a; }");
        updateUI();
        appendLog(QStringLiteral("=== 辨识完成，共 %1 个步骤 ===").arg(m_totalSteps));
        emit identificationComplete();
        return;
    }

    auto& step = m_steps[m_currentStep];
    step.status = IdentificationStepStatus::Running;
    updateStepStatus(m_currentStep, IdentificationStepStatus::Running);

    m_progressBar->setValue(m_currentStep * 100 / m_totalSteps);
    m_statusLabel->setText(tr("正在执行: %1").arg(step.name));
    m_statusLabel->setStyleSheet("QLabel { font-size: 14px; font-weight: bold; color: #ffa726; }");

    appendLog(QStringLiteral("──────────────────────────────"));
    appendLog(QStringLiteral("▶ 步骤 %1/%2: %3").arg(m_currentStep + 1).arg(m_totalSteps).arg(step.name));
    appendLog(QStringLiteral("  描述: %1").arg(step.description));

    // 发送辨识命令到 MCU
    // 命令格式: 0x50 + stepIndex
    QByteArray cmd;
    cmd.append(static_cast<char>(0x50));
    cmd.append(static_cast<char>(m_currentStep));
    emit commandRequested(cmd);
    appendLog(QStringLiteral("  → 发送命令: 0x50 0x%1").arg(m_currentStep, 2, 16, QChar('0')));
}

void MotorIdentificationWidget::handleResponse(uint8_t cmd, const QByteArray& data)
{
    if (!m_running) return;

    if (cmd == 0x50) {
        // 辨识步骤响应
        // 格式: [stepIndex, status, float_val(4 bytes)]
        if (data.size() < 6) {
            appendLog(QStringLiteral("  ✗ 响应数据长度不足"));
            return;
        }

        int stepIndex = static_cast<uint8_t>(data[0]);
        uint8_t status = static_cast<uint8_t>(data[1]);

        // 解析 float 值 (小端)
        float value = 0.0f;
        memcpy(&value, data.constData() + 2, sizeof(float));

        if (stepIndex != m_currentStep) {
            appendLog(QStringLiteral("  ⚠ 步骤索引不匹配: 期望 %1, 收到 %2").arg(m_currentStep).arg(stepIndex));
        }

        if (status == 0) {
            // 成功
            updateStepStatus(stepIndex, IdentificationStepStatus::Complete, static_cast<double>(value));
            appendLog(QStringLiteral("  ✓ 完成 — 值: %1 %2").arg(value, 0, 'f', 4).arg(m_steps[stepIndex].unit));
        } else {
            // 失败
            updateStepStatus(stepIndex, IdentificationStepStatus::Failed);
            appendLog(QStringLiteral("  ✗ 失败 — 错误码: %1").arg(status));
        }

        m_currentStep++;
        m_progressBar->setValue(m_currentStep * 100 / m_totalSteps);

        // 使用 QTimer 延迟执行下一步，避免阻塞
        QTimer::singleShot(500, this, &MotorIdentificationWidget::executeNextStep);
    }
}

void MotorIdentificationWidget::updateStepStatus(int stepIndex, IdentificationStepStatus st, double val)
{
    if (stepIndex < 0 || stepIndex >= m_steps.size()) return;
    m_steps[stepIndex].status = st;
    if (st == IdentificationStepStatus::Complete) {
        m_steps[stepIndex].value = val;
    }
    updateUI();
}

void MotorIdentificationWidget::appendLog(const QString& text)
{
    QString timestamp = QDateTime::currentDateTime().toString("HH:mm:ss.zzz");
    m_logView->append(QStringLiteral("[%1] %2").arg(timestamp, text));
    // 自动滚动到底部
    QScrollBar* sb = m_logView->verticalScrollBar();
    if (sb) {
        sb->setValue(sb->maximum());
    }
}