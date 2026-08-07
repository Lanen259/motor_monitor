#pragma once

#include <QWidget>
#include <QFrame>
#include <QVector>
#include <QLabel>
#include <QString>
#include <QGridLayout>
#include <QTimer>

namespace MotorStudio {

// Card type determines rendering style and threshold behavior
enum class CardType {
    MotorState,      // Indicator: Stopped / Running / Fault
    CommState,       // Indicator: Disconnected / Connected / Timeout
    Voltage,
    Current,
    Speed,
    Temperature,
    FaultCount,
    Generic
};

// Severity level for card background coloring
enum class Severity {
    Normal,
    Warning,
    Critical
};

// Per-card threshold configuration (numeric cards only)
struct CardThreshold {
    float warnLow  = -1e9f;
    float warnHigh =  1e9f;
    float critLow  = -1e9f;
    float critHigh =  1e9f;
};

// Single dashboard card with title bar, large value, unit, and severity coloring.
// Replaces the legacy DashboardCell with richer visuals and adaptive sizing.
class DashboardCard : public QFrame {
    Q_OBJECT
public:
    explicit DashboardCard(const QString& title, CardType type, QWidget* parent = nullptr);

    void setValue(float value);
    void setUnit(const QString& unit);
    void setThreshold(const CardThreshold& thresh);

    float value() const { return m_value; }
    CardType cardType() const { return m_type; }

protected:
    void resizeEvent(QResizeEvent* event) override;

private:
    void updateAppearance();
    void applySeverity(Severity sev);
    Severity evaluateSeverity() const;
    QString formatValue() const;
    void scaleFonts();

    CardType m_type;
    QString m_title;
    QString m_unit;
    float m_value = 0;
    CardThreshold m_threshold;
    Severity m_currentSeverity = Severity::Normal;

    QLabel* m_titleLabel;
    QLabel* m_valueLabel;
    QLabel* m_unitLabel;
};

// Dashboard panel managing DashboardCards in a responsive grid.
// Cards auto-populate from DataBus subscriptions and scale with the parent widget.
class DashboardWidget : public QWidget {
    Q_OBJECT
public:
    explicit DashboardWidget(QWidget* parent = nullptr);

    // Legacy cell management (backward compat — delegates to DashboardCard)
    int addCell(const QString& title, const QString& unit = "");
    void removeCell(int index);
    void clearAll();
    int cellCount() const { return m_cards.size(); }

    // Direct value update (legacy mode)
    void updateValues(const QVector<float>& values);
    void updateValue(int index, float value);

    // DataBus subscription mode — auto-creates typed cards from TopicRegistry
    void subscribeToDataBus(int fps = 10);

    // Warning thresholds (pass-through to per-card CardThreshold)
    void setWarningThreshold(int index, float min, float max);

    // Column count (adaptive to widget width — user override)
    void setColumns(int cols);

protected:
    void resizeEvent(QResizeEvent* event) override;

private slots:
    void onRefreshTimer();

private:
    void recalculateLayout();
    static CardType detectCardType(const QString& topicName);
    static CardThreshold defaultThreshold(CardType type);

    QVector<DashboardCard*> m_cards;
    QGridLayout* m_grid;
    int m_columns = 3;
    QTimer* m_refreshTimer = nullptr;
};

} // namespace MotorStudio
