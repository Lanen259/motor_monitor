#include "DashboardWidget.h"
#include "../databus/DataBus.h"
#include "../databus/Topic.h"
#include <QPainter>
#include <QVBoxLayout>
#include <QFont>
#include <QResizeEvent>
#include <QFontMetrics>
#include <algorithm>

namespace MotorStudio {

// ============================================================
// DashboardCard — single indicator/value card
// ============================================================

DashboardCard::DashboardCard(const QString& title, CardType type, QWidget* parent)
    : QFrame(parent)
    , m_type(type)
    , m_title(title)
{
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setMinimumSize(140, 110);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(10, 6, 10, 8);
    layout->setSpacing(2);

    // Title label — small, dim, top-aligned
    m_titleLabel = new QLabel(title, this);
    m_titleLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    m_titleLabel->setWordWrap(false);
    m_titleLabel->setFixedHeight(18);
    m_titleLabel->setStyleSheet(
        "QLabel { color: #757575; background: transparent; font-family: 'Segoe UI'; }");

    // Value label — large, centered, expands to fill available space
    m_valueLabel = new QLabel("--", this);
    m_valueLabel->setAlignment(Qt::AlignCenter);
    m_valueLabel->setWordWrap(false);
    m_valueLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    // Unit label — small, centered, shown for numeric cards only
    m_unitLabel = new QLabel(this);
    m_unitLabel->setAlignment(Qt::AlignCenter);
    m_unitLabel->setFixedHeight(16);
    m_unitLabel->setStyleSheet(
        "QLabel { color: #757575; background: transparent; font-family: 'Segoe UI'; }");

    layout->addWidget(m_titleLabel);
    layout->addWidget(m_valueLabel, 1);   // stretch factor 1 — fills remaining space
    layout->addWidget(m_unitLabel);

    applySeverity(Severity::Normal);
    scaleFonts();
}

void DashboardCard::setValue(float value)
{
    m_value = value;
    updateAppearance();
}

void DashboardCard::setUnit(const QString& unit)
{
    m_unit = unit;
    m_unitLabel->setText(unit);
    bool showUnit = (m_type != CardType::MotorState && m_type != CardType::CommState);
    m_unitLabel->setVisible(showUnit && !unit.isEmpty());
}

void DashboardCard::setThreshold(const CardThreshold& thresh)
{
    m_threshold = thresh;
    // Re-evaluate severity with new thresholds
    updateAppearance();
}

// ============================================================
// Severity evaluation & appearance
// ============================================================

Severity DashboardCard::evaluateSeverity() const
{
    switch (m_type) {
    case CardType::MotorState: {
        int state = static_cast<int>(m_value);
        // 0=Stopped, 1=Running, 2=Fault
        if (state == 2) return Severity::Critical;
        return Severity::Normal;
    }
    case CardType::CommState: {
        int state = static_cast<int>(m_value);
        // 0=Disconnected, 1=Connected, 2=Timeout
        if (state == 2) return Severity::Critical;
        if (state == 0) return Severity::Warning;
        return Severity::Normal;
    }
    default:
        break;
    }

    // Numeric thresholds
    if (m_value < m_threshold.critLow || m_value > m_threshold.critHigh)
        return Severity::Critical;
    if (m_value < m_threshold.warnLow || m_value > m_threshold.warnHigh)
        return Severity::Warning;
    return Severity::Normal;
}

void DashboardCard::applySeverity(Severity sev)
{
    QString bgColor, accentColor, textColor;

    switch (sev) {
    case Severity::Normal:
        bgColor     = "#E8F5E9";
        accentColor = "#4CAF50";
        textColor   = "#4CAF50";
        break;
    case Severity::Warning:
        bgColor     = "#FFF8E1";
        accentColor = "#FF9800";
        textColor   = "#FF9800";
        break;
    case Severity::Critical:
        bgColor     = "#FFEBEE";
        accentColor = "#F44336";
        textColor   = "#F44336";
        break;
    }

    setStyleSheet(QString(
        "DashboardCard {"
        "  background-color: %1;"
        "  border: 1px solid #E0E0E0;"
        "  border-left: 4px solid %2;"
        "  border-radius: 6px;"
        "}"
    ).arg(bgColor, accentColor));

    m_valueLabel->setStyleSheet(QString(
        "QLabel { color: %1; background: transparent; font-family: 'Consolas'; }"
    ).arg(textColor));
}

QString DashboardCard::formatValue() const
{
    switch (m_type) {
    case CardType::MotorState: {
        int state = static_cast<int>(m_value);
        switch (state) {
        case 0: return QStringLiteral("STOPPED");
        case 1: return QStringLiteral("RUNNING");
        case 2: return QStringLiteral("FAULT");
        default: return QString::number(state);
        }
    }
    case CardType::CommState: {
        int state = static_cast<int>(m_value);
        switch (state) {
        case 0: return QStringLiteral("DISCONNECTED");
        case 1: return QStringLiteral("CONNECTED");
        case 2: return QStringLiteral("TIMEOUT");
        default: return QString::number(state);
        }
    }
    default:
        // Numeric: 2 decimal places
        return QString::number(m_value, 'f', 2);
    }
}

void DashboardCard::updateAppearance()
{
    Severity sev = evaluateSeverity();
    if (sev != m_currentSeverity) {
        m_currentSeverity = sev;
        applySeverity(sev);
    }

    m_valueLabel->setText(formatValue());

    // Unit visibility for state cards vs numeric cards
    bool showUnit = (m_type != CardType::MotorState && m_type != CardType::CommState);
    m_unitLabel->setVisible(showUnit && !m_unit.isEmpty());
}

// ============================================================
// Adaptive font scaling — fonts scale with card size
// ============================================================

void DashboardCard::scaleFonts()
{
    int h = height();
    int w = width();

    // Value font: scales proportionally to card height
    int valuePt = qBound(14, h / 6, 36);
    QFont valueFont("Consolas", valuePt, QFont::Bold);
    m_valueLabel->setFont(valueFont);

    // Title font: small, scales mildly
    int titlePt = qBound(8, h / 18, 12);
    QFont titleFont("Segoe UI", titlePt);
    m_titleLabel->setFont(titleFont);
    // Update title fixed height based on font
    QFontMetrics fm(titleFont);
    m_titleLabel->setFixedHeight(fm.height() + 2);

    // Unit font
    int unitPt = qBound(7, h / 20, 11);
    QFont unitFont("Segoe UI", unitPt);
    m_unitLabel->setFont(unitFont);
    QFontMetrics ufm(unitFont);
    m_unitLabel->setFixedHeight(ufm.height() + 2);

    Q_UNUSED(w);
}

void DashboardCard::resizeEvent(QResizeEvent* event)
{
    QFrame::resizeEvent(event);
    scaleFonts();
}

// ============================================================
// DashboardWidget — responsive grid of DashboardCards
// ============================================================

DashboardWidget::DashboardWidget(QWidget* parent)
    : QWidget(parent)
    , m_columns(3)
{
    m_grid = new QGridLayout(this);
    m_grid->setSpacing(8);
    m_grid->setContentsMargins(8, 8, 8, 8);
    setStyleSheet("background-color: #F5F7FA;");
}

// ============================================================
// Card-type detection from topic name
// ============================================================

CardType DashboardWidget::detectCardType(const QString& topicName)
{
    QString lower = topicName.trimmed().toLower();

    // Motor state: contains "motor" or "state" (but not "comm")
    if (lower.contains("motor") || (lower.contains("state") && !lower.contains("comm")))
        return CardType::MotorState;

    // Comm state
    if (lower.contains("comm") || lower.contains("connect"))
        return CardType::CommState;

    // Voltage
    if (lower.contains("volt") || lower.contains("vdc") || lower.contains("vbus")
        || lower.contains("v_a") || lower.contains("va ") || lower.startsWith("v "))
        return CardType::Voltage;

    // Current
    if (lower.contains("curr") || lower.contains("amp")
        || lower == "ia" || lower == "ib" || lower == "ic"
        || lower == "id" || lower == "iq"
        || lower.startsWith("i_") || lower.startsWith("i "))
        return CardType::Current;

    // Speed / RPM
    if (lower.contains("speed") || lower.contains("rpm") || lower.contains("vel"))
        return CardType::Speed;

    // Temperature
    if (lower.contains("temp") || lower.contains("heat") || lower.contains("deg"))
        return CardType::Temperature;

    // Fault / Error count
    if (lower.contains("fault") || lower.contains("error"))
        return CardType::FaultCount;

    return CardType::Generic;
}

// ============================================================
// Default thresholds per card type
// ============================================================

CardThreshold DashboardWidget::defaultThreshold(CardType type)
{
    CardThreshold t;

    switch (type) {
    case CardType::Voltage:
        // Nominal ~24V, warn at 20/28, critical at 16/32
        t.warnLow  = 20.0f;  t.warnHigh = 28.0f;
        t.critLow  = 16.0f;  t.critHigh = 32.0f;
        break;

    case CardType::Current:
        t.warnHigh = 8.0f;   t.critHigh = 10.0f;
        break;

    case CardType::Speed:
        t.warnHigh = 3200.0f; t.critHigh = 3500.0f;
        break;

    case CardType::Temperature:
        t.warnHigh = 60.0f;  t.critHigh = 80.0f;
        break;

    case CardType::FaultCount:
        t.warnHigh = 1.0f;   t.critHigh = 3.0f;
        break;

    case CardType::MotorState:
    case CardType::CommState:
        // State cards use semantic evaluation, not numeric thresholds
        break;

    case CardType::Generic:
        break;
    }

    return t;
}

// ============================================================
// DataBus subscription — auto-creates typed cards
// ============================================================

void DashboardWidget::subscribeToDataBus(int fps)
{
    clearAll();

    auto& registry = TopicRegistry::instance();
    auto topicIds = registry.allTopicIds();

    for (auto tid : topicIds) {
        ChannelDescriptor desc = registry.descriptor(tid);
        QString name = QString::fromStdString(desc.name);
        QString unit = QString::fromStdString(desc.unit);

        CardType type = detectCardType(name);

        auto* card = new DashboardCard(name, type, this);
        card->setUnit(unit);
        card->setThreshold(defaultThreshold(type));
        m_cards.append(card);
    }

    recalculateLayout();

    // Start/restart refresh timer
    if (!m_refreshTimer) {
        m_refreshTimer = new QTimer(this);
        connect(m_refreshTimer, &QTimer::timeout, this, &DashboardWidget::onRefreshTimer);
    }
    m_refreshTimer->start(1000 / fps);
}

void DashboardWidget::onRefreshTimer()
{
    auto& registry = TopicRegistry::instance();
    auto& bus = DataBus::instance();
    auto topicIds = registry.allTopicIds();

    for (size_t i = 0; i < topicIds.size() && i < static_cast<size_t>(m_cards.size()); ++i) {
        ChannelDescriptor desc = registry.descriptor(topicIds[i]);

        // Sync unit from registry (supports dynamic descriptor updates via ChannelConfigDialog)
        if (!desc.unit.empty()) {
            m_cards[i]->setUnit(QString::fromStdString(desc.unit));
        }

        // Update value from DataBus
        auto val = bus.latestValue(topicIds[i]);
        if (val.has_value()) {
            m_cards[i]->setValue(val.value());
        }
    }
}

// ============================================================
// Responsive layout — recalculates columns based on widget width
// ============================================================

void DashboardWidget::recalculateLayout()
{
    int availWidth = width() - m_grid->contentsMargins().left() - m_grid->contentsMargins().right();
    int newCols = qMax(2, availWidth / 220);

    if (newCols == m_columns && m_cards.size() == static_cast<int>(m_grid->count())) {
        return;  // No re-layout needed
    }

    m_columns = newCols;

    // Remove all cards from layout (but keep widgets)
    for (auto* card : m_cards) {
        m_grid->removeWidget(card);
    }

    // Re-add in grid order
    for (int i = 0; i < m_cards.size(); ++i) {
        int row = i / m_columns;
        int col = i % m_columns;
        m_grid->addWidget(m_cards[i], row, col);
    }
}

void DashboardWidget::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    recalculateLayout();
}

// ============================================================
// Legacy API — backward compatible delegates
// ============================================================

int DashboardWidget::addCell(const QString& title, const QString& unit)
{
    auto* card = new DashboardCard(title, CardType::Generic, this);
    card->setUnit(unit);
    int index = m_cards.size();
    m_cards.append(card);

    int row = index / m_columns;
    int col = index % m_columns;
    m_grid->addWidget(card, row, col);

    return index;
}

void DashboardWidget::removeCell(int index)
{
    if (index < 0 || index >= m_cards.size()) return;
    m_grid->removeWidget(m_cards[index]);
    delete m_cards[index];
    m_cards.removeAt(index);
}

void DashboardWidget::clearAll()
{
    for (auto* card : m_cards) {
        m_grid->removeWidget(card);
        delete card;
    }
    m_cards.clear();
}

void DashboardWidget::updateValues(const QVector<float>& values)
{
    while (m_cards.size() < values.size()) {
        addCell(QString("CH%1").arg(m_cards.size() + 1));
    }

    for (int i = 0; i < values.size() && i < m_cards.size(); ++i) {
        m_cards[i]->setValue(values[i]);
    }
}

void DashboardWidget::updateValue(int index, float value)
{
    if (index >= 0 && index < m_cards.size()) {
        m_cards[index]->setValue(value);
    }
}

void DashboardWidget::setWarningThreshold(int index, float min, float max)
{
    if (index >= 0 && index < m_cards.size()) {
        CardThreshold t = m_cards[index]->cardType() == CardType::Generic
            ? CardThreshold{min, max, min, max}  // legacy: warn == crit
            : defaultThreshold(m_cards[index]->cardType());
        t.warnLow  = min;
        t.warnHigh = max;
        m_cards[index]->setThreshold(t);
    }
}

void DashboardWidget::setColumns(int cols)
{
    m_columns = qMax(1, cols);
    recalculateLayout();
}

} // namespace MotorStudio
