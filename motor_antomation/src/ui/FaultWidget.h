#pragma once

#include <QWidget>
#include <QVector>
#include <QString>
#include <QDateTime>
#include <QListWidget>
#include <QLabel>

namespace MotorStudio {

// ============================================================
// 故障条目
// ============================================================
struct FaultEntry {
    QString code;
    QString message;
    QDateTime timestamp;
    bool active = true;
    bool isHardware = false;  // 硬件故障 = 需要立即停机
};

// ============================================================
// 故障管理面板
// ============================================================
class FaultWidget : public QWidget {
    Q_OBJECT
public:
    explicit FaultWidget(QWidget* parent = nullptr);

    // 故障管理
    void addFault(const FaultEntry& fault);
    void clearFault(const QString& code);
    void clearAllFaults();

    int activeFaultCount() const;
    bool hasHardwareFault() const;

    // 模式切换
    void setTestMode(bool testMode);
    bool isTestMode() const { return m_testMode; }

signals:
    void faultAdded(const FaultEntry& fault);
    void faultCleared(const QString& code);
    void hardwareFaultOccurred(const FaultEntry& fault);  // 硬件故障 - 需立即停机
    void faultCountChanged(int activeCount);

private:
    void refreshDisplay();

    QVector<FaultEntry> m_faults;
    QListWidget* m_faultList;
    QLabel* m_statusLabel;
    QLabel* m_countLabel;
    bool m_testMode;
};

} // namespace MotorStudio