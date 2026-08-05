#include "CurveWidget.h"
#include <QPainter>
#include <QPaintEvent>
#include <QWheelEvent>
#include <QMouseEvent>
#include <QToolTip>
#include <cmath>
#include <algorithm>
#include <QDateTime>

namespace MotorStudio {

CurveWidget::CurveWidget(QWidget* parent)
    : QWidget(parent)
    , m_autoScale(true)
    , m_yMin(0)
    , m_yMax(100)
    , m_xRangeSeconds(10.0)
    , m_t0(0)
    , m_dragging(false)
    , m_panning(false)
{
    setMouseTracking(true);
    setMinimumSize(200, 150);
    setStyleSheet("background-color: #1a1a2e;");
}

int CurveWidget::addChannel(const QString& name, const QColor& color)
{
    Channel ch;
    ch.name = name;
    ch.color = color;
    m_channels.append(ch);
    return m_channels.size() - 1;
}

void CurveWidget::removeChannel(int index)
{
    if (index >= 0 && index < m_channels.size()) {
        m_channels.removeAt(index);
    }
}

void CurveWidget::clearAllChannels()
{
    m_channels.clear();
    m_t0 = 0;
    update();
}

void CurveWidget::pushData(int channelIndex, float value, uint64_t timestampUs)
{
    if (channelIndex < 0 || channelIndex >= m_channels.size()) return;

    if (timestampUs == 0) {
        timestampUs = QDateTime::currentMSecsSinceEpoch() * 1000ULL;
    }

    if (m_t0 == 0) {
        m_t0 = timestampUs;
    }

    double t = (timestampUs - m_t0) / 1000000.0;  // 转换为秒
    m_channels[channelIndex].data.append(QPointF(t, value));

    // 更新通道范围
    auto& ch = m_channels[channelIndex];
    if (ch.data.size() == 1) {
        ch.minVal = ch.maxVal = value;
    } else {
        ch.minVal = std::min(ch.minVal, value);
        ch.maxVal = std::max(ch.maxVal, value);
    }
}

void CurveWidget::pushFrame(const QVector<float>& values, uint64_t timestampUs)
{
    if (timestampUs == 0) {
        timestampUs = QDateTime::currentMSecsSinceEpoch() * 1000ULL;
    }

    while (m_channels.size() < values.size()) {
        addChannel(QString("CH%1").arg(m_channels.size() + 1));
    }

    for (int i = 0; i < values.size(); ++i) {
        pushData(i, values[i], timestampUs);
    }

    update();  // 触发重绘
}

void CurveWidget::setYRange(float min, float max)
{
    m_yMin = min;
    m_yMax = max;
    m_autoScale = false;
}

void CurveWidget::setXRangeSeconds(double seconds)
{
    m_xRangeSeconds = std::max(0.5, seconds);
}

void CurveWidget::setChannelColor(int index, const QColor& color)
{
    if (index >= 0 && index < m_channels.size()) {
        m_channels[index].color = color;
    }
}

QColor CurveWidget::channelColor(int index) const
{
    if (index >= 0 && index < m_channels.size()) {
        return m_channels[index].color;
    }
    return Qt::white;
}

void CurveWidget::clearData()
{
    for (auto& ch : m_channels) {
        ch.data.clear();
        ch.minVal = ch.maxVal = 0;
    }
    m_t0 = 0;
    update();
}

void CurveWidget::saveScreenshot(const QString& filePath)
{
    QPixmap pix = grab();
    pix.save(filePath, "PNG");
}

// ============================================================
// 绘制
// ============================================================

void CurveWidget::paintEvent(QPaintEvent* /*event*/)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    QRect plotRect = rect().adjusted(60, 20, -20, -40);

    drawGrid(painter, plotRect);
    drawCurves(painter, plotRect);
    drawAxisLabels(painter, plotRect);
    drawLegend(painter);
}

void CurveWidget::drawGrid(QPainter& painter, const QRect& rect)
{
    painter.setPen(QPen(QColor(50, 50, 70), 1, Qt::DashLine));

    // 水平网格线
    int hLines = 5;
    for (int i = 0; i <= hLines; ++i) {
        int y = rect.top() + rect.height() * i / hLines;
        painter.drawLine(rect.left(), y, rect.right(), y);
    }

    // 垂直网格线
    int vLines = 5;
    for (int i = 0; i <= vLines; ++i) {
        int x = rect.left() + rect.width() * i / vLines;
        painter.drawLine(x, rect.top(), x, rect.bottom());
    }

    // 边框
    painter.setPen(QPen(QColor(80, 80, 100), 1));
    painter.drawRect(rect);
}

void CurveWidget::drawCurves(QPainter& painter, const QRect& rect)
{
    if (m_autoScale) {
        updateAutoScale();
    }

    for (int ci = 0; ci < m_channels.size(); ++ci) {
        const auto& ch = m_channels[ci];
        if (!ch.visible || ch.data.isEmpty()) continue;

        QPen pen(ch.color, 1.5);
        painter.setPen(pen);

        int totalPoints = ch.data.size();
        if (totalPoints == 0) continue;

        // 降采样：如果数据点超过最大绘制点数，均匀采样
        int step = std::max(1, totalPoints / kMaxDrawPoints);

        QPointF prev;
        bool first = true;

        for (int i = 0; i < totalPoints; i += step) {
            QPointF pixel = dataToPixel(ch.data[i], rect);
            if (first) {
                first = false;
            } else {
                painter.drawLine(prev, pixel);
            }
            prev = pixel;
        }

        // 确保最后一个点被绘制
        if (totalPoints > 1 && (totalPoints - 1) % step != 0) {
            QPointF lastPixel = dataToPixel(ch.data.last(), rect);
            painter.drawLine(prev, lastPixel);
        }
    }
}

void CurveWidget::drawLegend(QPainter& painter)
{
    int x = rect().left() + 70;
    int y = rect().top() + 5;

    painter.setFont(QFont("Consolas", 8));

    for (int i = 0; i < m_channels.size(); ++i) {
        const auto& ch = m_channels[i];
        if (!ch.visible) continue;

        // 颜色块
        painter.fillRect(x, y, 12, 12, ch.color);
        painter.setPen(Qt::white);

        // 通道名 + 最新值
        float latest = ch.data.isEmpty() ? 0.0f : ch.data.last().y();
        QString text = QString("%1: %2").arg(ch.name).arg(latest, 6, 'f', 2);
        painter.drawText(x + 16, y + 10, text);

        x += 16 + painter.fontMetrics().width(text) + 20;
        if (x > rect().right() - 100) {
            x = rect().left() + 70;
            y += 16;
        }
    }
}

void CurveWidget::drawAxisLabels(QPainter& painter, const QRect& rect)
{
    painter.setPen(QColor(150, 150, 170));
    painter.setFont(QFont("Consolas", 8));

    // Y轴标签
    float range = m_yMax - m_yMin;
    if (range <= 0) range = 1;
    int hLines = 5;
    for (int i = 0; i <= hLines; ++i) {
        float val = m_yMax - range * i / hLines;
        int y = rect.top() + rect.height() * i / hLines;
        QString label = QString::number(val, 'f', 1);
        painter.drawText(rect.left() - 55, y + 4, 50, 12, Qt::AlignRight | Qt::AlignVCenter, label);
    }

    // X轴标签
    int vLines = 5;
    for (int i = 0; i <= vLines; ++i) {
        double t = m_xRangeSeconds * i / vLines;
        int x = rect.left() + rect.width() * i / vLines;
        QString label = QString::number(t, 'f', 1) + "s";
        painter.drawText(x - 20, rect.bottom() + 2, 40, 12, Qt::AlignHCenter, label);
    }
}

void CurveWidget::updateAutoScale()
{
    if (m_channels.isEmpty()) {
        m_yMin = -10;
        m_yMax = 10;
        return;
    }

    float globalMin = 1e9f, globalMax = -1e9f;
    for (const auto& ch : m_channels) {
        if (!ch.visible || ch.data.isEmpty()) continue;
        globalMin = std::min(globalMin, ch.minVal);
        globalMax = std::max(globalMax, ch.maxVal);
    }

    if (globalMin >= globalMax) {
        m_yMin = globalMin - 1;
        m_yMax = globalMax + 1;
    } else {
        float margin = (globalMax - globalMin) * 0.1f;
        m_yMin = globalMin - margin;
        m_yMax = globalMax + margin;
    }
}

QPointF CurveWidget::dataToPixel(const QPointF& dataPoint, const QRect& rect) const
{
    double x = rect.left() + (dataPoint.x() / m_xRangeSeconds) * rect.width();
    double y = rect.bottom() - ((dataPoint.y() - m_yMin) / (m_yMax - m_yMin)) * rect.height();
    return QPointF(x, y);
}

QPointF CurveWidget::pixelToData(const QPointF& pixel, const QRect& rect) const
{
    double t = (pixel.x() - rect.left()) / rect.width() * m_xRangeSeconds;
    double v = m_yMax - (pixel.y() - rect.top()) / rect.height() * (m_yMax - m_yMin);
    return QPointF(t, v);
}

// ============================================================
// 鼠标交互
// ============================================================

void CurveWidget::resizeEvent(QResizeEvent* /*event*/)
{
    update();
}

void CurveWidget::wheelEvent(QWheelEvent* event)
{
    double factor = (event->angleDelta().y() > 0) ? 0.9 : 1.1;
    m_xRangeSeconds = std::max(0.5, m_xRangeSeconds * factor);
    update();
}

void CurveWidget::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::RightButton) {
        m_panning = true;
        m_lastMousePos = event->pos();
        setCursor(Qt::ClosedHandCursor);
    }
}

void CurveWidget::mouseMoveEvent(QMouseEvent* event)
{
    if (m_panning) {
        QPoint delta = event->pos() - m_lastMousePos;
        m_lastMousePos = event->pos();
        // 平移（调整t0）
        double dt = -delta.x() / (double)width() * m_xRangeSeconds;
        m_t0 = (m_t0 > dt * 1000000.0) ? m_t0 - dt * 1000000.0 : 0;
        update();
    }
}

void CurveWidget::mouseReleaseEvent(QMouseEvent* /*event*/)
{
    m_panning = false;
    setCursor(Qt::ArrowCursor);
}

} // namespace MotorStudio