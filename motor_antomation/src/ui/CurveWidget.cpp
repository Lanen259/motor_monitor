#include "CurveWidget.h"
#include "../curve/CurveEngine.h"
#include "../curve/TimeAxisManager.h"
#include <QPainter>
#include <QPaintEvent>
#include <QWheelEvent>
#include <QMouseEvent>
#include <QKeyEvent>
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
    setStyleSheet("background-color: #F5F7FA;");
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

    double t = (timestampUs - m_t0) / 1000000.0;
    m_channels[channelIndex].data.append(QPointF(t, value));

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

    update();
}

// ============================================================
// CurveEngine-backed pull mode (P0 target architecture)
// ============================================================

void CurveWidget::attachCurveEngine(CurveEngine* engine, int fps)
{
    m_curveEngine = engine;
    if (!m_pullTimer) {
        m_pullTimer = new QTimer(this);
        connect(m_pullTimer, &QTimer::timeout, this, &CurveWidget::onPullTimer);
    }
    m_pullTimer->start(1000 / fps);
}

void CurveWidget::detachCurveEngine()
{
    if (m_pullTimer) {
        m_pullTimer->stop();
    }
    m_curveEngine = nullptr;
}

void CurveWidget::setChannelTopicId(int index, uint32_t topicId)
{
    if (index >= 0 && index < m_channels.size()) {
        m_channels[index].topicId = topicId;
    }
}

void CurveWidget::setAutoPopulateChannels(bool enabled)
{
    m_autoPopulateChannels = enabled;
}

void CurveWidget::onPullTimer()
{
    if (!m_curveEngine) return;

    auto& registry = TopicRegistry::instance();

    if (m_autoPopulateChannels) {
        // Legacy mode: auto-populate all engine channels
        auto ids = m_curveEngine->channelIds();
        if (ids.empty()) return;

        // Sync channels: create if new CurveEngine channels appear
        while (static_cast<size_t>(m_channels.size()) < ids.size()) {
            int idx = m_channels.size();
            uint32_t tid = ids[idx];

            ChannelDescriptor desc = registry.descriptor(tid);
            QString name = QString::fromStdString(desc.name);
            if (name.isEmpty()) {
                name = QString("CH%1").arg(tid);
            }

            QColor color = Qt::cyan;
            if (desc.color != 0 && desc.color != 0xFF888888) {
                color = QColor((desc.color >> 16) & 0xFF, (desc.color >> 8) & 0xFF,
                               desc.color & 0xFF, (desc.color >> 24) & 0xFF);
            }

            addChannel(name, color);
            m_channels[idx].topicId = tid;
        }

        // Sync existing channel names and colors from registry (WI-005: dynamic update)
        for (size_t i = 0; i < ids.size() && i < static_cast<size_t>(m_channels.size()); ++i) {
            uint32_t tid = ids[i];
            auto& localCh = m_channels[i];

            ChannelDescriptor desc = registry.descriptor(tid);
            if (!desc.name.empty()) {
                localCh.name = QString::fromStdString(desc.name);
            }
            if (desc.color != 0 && desc.color != 0xFF888888) {
                localCh.color = QColor((desc.color >> 16) & 0xFF, (desc.color >> 8) & 0xFF,
                                        desc.color & 0xFF, (desc.color >> 24) & 0xFF);
            }
        }

        // Pull min/max from CurveEngine channels for auto-scale
        for (size_t i = 0; i < ids.size() && i < static_cast<size_t>(m_channels.size()); ++i) {
            uint32_t tid = ids[i];
            auto* ch = m_curveEngine->channel(tid);
            if (!ch) continue;

            auto range = ch->dataRange();
            m_channels[i].minVal = range.minVal;
            m_channels[i].maxVal = range.maxVal;
        }
    } else {
        // Externally-managed mode (PlotCell): only update existing channels with topicIds
        if (m_channels.isEmpty()) return;

        for (int i = 0; i < m_channels.size(); ++i) {
            auto& localCh = m_channels[i];
            if (localCh.topicId == 0) continue;

            ChannelDescriptor desc = registry.descriptor(localCh.topicId);
            if (!desc.name.empty()) {
                localCh.name = QString::fromStdString(desc.name);
            }
            if (desc.color != 0 && desc.color != 0xFF888888) {
                localCh.color = QColor((desc.color >> 16) & 0xFF, (desc.color >> 8) & 0xFF,
                                        desc.color & 0xFF, (desc.color >> 24) & 0xFF);
            }

            auto* ceCh = m_curveEngine->channel(localCh.topicId);
            if (!ceCh) continue;

            auto range = ceCh->dataRange();
            localCh.minVal = range.minVal;
            localCh.maxVal = range.maxVal;
        }
    }

    update();
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

void CurveWidget::setTimeBase(uint64_t t0)
{
    m_t0 = t0;
    update();
}

QColor CurveWidget::channelColor(int index) const
{
    if (index >= 0 && index < m_channels.size()) {
        return m_channels[index].color;
    }
    return Qt::white;
}

QString CurveWidget::channelName(int index) const
{
    if (index >= 0 && index < m_channels.size()) {
        return m_channels[index].name;
    }
    return QString();
}

void CurveWidget::setChannelVisible(int index, bool visible)
{
    if (index >= 0 && index < m_channels.size()) {
        m_channels[index].visible = visible;
        update();
    }
}

bool CurveWidget::isChannelVisible(int index) const
{
    if (index >= 0 && index < m_channels.size()) {
        return m_channels[index].visible;
    }
    return false;
}

uint32_t CurveWidget::channelTopicId(int index) const
{
    if (index >= 0 && index < m_channels.size()) {
        return m_channels[index].topicId;
    }
    return 0;
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
// TimeAxisManager 集成 (WI-103)
// ============================================================

void CurveWidget::setTimeAxisManager(TimeAxisManager* manager)
{
    if (m_timeAxisManager == manager) return;

    // 断开旧连接
    if (m_timeAxisManager) {
        disconnect(m_timeAxisManager, &TimeAxisManager::sharedRangeChanged,
                   this, &CurveWidget::onSharedRangeChanged);
    }

    m_timeAxisManager = manager;

    // 连接新管理器
    if (m_timeAxisManager) {
        connect(m_timeAxisManager, &TimeAxisManager::sharedRangeChanged,
                this, &CurveWidget::onSharedRangeChanged);
    }
}

void CurveWidget::setTimeSynced(bool sync)
{
    if (m_timeSynced == sync) return;
    m_timeSynced = sync;

    // 切换到同步模式时，立即采用共享时间范围
    if (m_timeSynced && m_timeAxisManager) {
        TimeRange range = m_timeAxisManager->sharedRange();
        m_t0 = range.t0;
        m_xRangeSeconds = range.xRangeSeconds;
        update();
    }
}

void CurveWidget::onSharedRangeChanged(uint64_t t0, double xRangeSeconds)
{
    // 独立模式不响应共享范围变更
    if (!m_timeSynced) return;

    // 防止反馈循环：标记为管理器更新中，不重复发射信号
    m_updatingFromManager = true;

    m_t0 = t0;
    m_xRangeSeconds = xRangeSeconds;
    update();

    m_updatingFromManager = false;
}

void CurveWidget::notifyTimeAxisChange()
{
    // 防止反馈循环：如果正在响应管理器更新，不重复发射信号
    if (m_updatingFromManager) return;

    // 同步模式且有管理器：向管理器传播变更
    if (m_timeSynced && m_timeAxisManager) {
        m_timeAxisManager->updateSharedRange(m_t0, m_xRangeSeconds);
        // 管理器会通过 sharedRangeChanged 信号回传更新给所有同步部件
        // 为避免重复绘制，这里不再 emit timeAxisChanged
        return;
    }

    // 独立模式或无管理器：发射本地信号（向后兼容）
    emit timeAxisChanged();
}

// ============================================================
// Paint
// ============================================================

void CurveWidget::paintEvent(QPaintEvent* /*event*/)
{
    // WI-010: Frame timing measurement
    if (!m_frameTimer.isValid()) {
        m_frameTimer.start();
    } else {
        m_frameIntervalMs = m_frameTimer.restart();
    }

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    QRect plotRect = rect().adjusted(60, 20, -20, -40);

    drawGrid(painter, plotRect);
    drawCurves(painter, plotRect);
    drawAxisLabels(painter, plotRect);
    drawLegend(painter);

    // WI-104: 框选缩放矩形
    if (m_rubberBanding && m_rubberBandRect.width() > 2 && m_rubberBandRect.height() > 2) {
        QPen rubberPen(QColor("#2196F3"), 1.5, Qt::DashLine);
        painter.setPen(rubberPen);
        QColor rubberFill(32, 0, 0, 255);  // RGBA: transparent blue (#200000FF)
        painter.setBrush(rubberFill);
        painter.drawRect(m_rubberBandRect);
    }
}

void CurveWidget::drawGrid(QPainter& painter, const QRect& rect)
{
    painter.setPen(QPen(QColor(220, 220, 230), 1, Qt::DashLine));

    int hLines = 5;
    for (int i = 0; i <= hLines; ++i) {
        int y = rect.top() + rect.height() * i / hLines;
        painter.drawLine(rect.left(), y, rect.right(), y);
    }

    int vLines = 5;
    for (int i = 0; i <= vLines; ++i) {
        int x = rect.left() + rect.width() * i / vLines;
        painter.drawLine(x, rect.top(), x, rect.bottom());
    }

    painter.setPen(QPen(QColor(200, 200, 210), 1));
    painter.drawRect(rect);
}

void CurveWidget::drawCurves(QPainter& painter, const QRect& rect)
{
    if (m_autoScale) {
        updateAutoScale();
    }

    for (int ci = 0; ci < m_channels.size(); ++ci) {
        const auto& ch = m_channels[ci];
        if (!ch.visible) continue;

        QPen pen(ch.color, 1.5);
        painter.setPen(pen);

        // WI-010: CurveEngine-backed mode → LTTB downsample targeting pixel width
        if (m_curveEngine && ch.topicId != 0) {
            auto* ceCh = m_curveEngine->channel(ch.topicId);
            if (!ceCh || ceCh->count() == 0) continue;

            // Initialize time base from data
            if (m_t0 == 0) {
                auto range = ceCh->dataRange();
                m_t0 = range.minTime;
            }

            // LTTB downsample to pixel width (avoids rendering invisible detail)
            size_t targetPts = static_cast<size_t>(std::max(2, rect.width()));
            auto downsampled = m_curveEngine->downsample(ch.topicId, targetPts);
            if (downsampled.size() < 2) continue;

            QPointF prev;
            bool first = true;
            for (const auto& p : downsampled) {
                double t = (p.first - m_t0) / 1000000.0;
                QPointF pixel = dataToPixel(QPointF(t, p.second), rect);
                if (first) {
                    first = false;
                } else {
                    painter.drawLine(prev, pixel);
                }
                prev = pixel;
            }
            continue;
        }

        // Legacy push mode: render all local data points
        if (ch.data.isEmpty()) continue;

        QPointF prev;
        bool first = true;
        for (const auto& pt : ch.data) {
            QPointF pixel = dataToPixel(pt, rect);
            if (first) {
                first = false;
            } else {
                painter.drawLine(prev, pixel);
            }
            prev = pixel;
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

        painter.fillRect(x, y, 12, 12, ch.color);
        painter.setPen(QColor("#212121"));

        float latest = 0.0f;
        if (!ch.data.isEmpty()) {
            latest = ch.data.last().y();
        } else if (m_curveEngine && ch.topicId != 0) {
            // CurveEngine mode: get latest value from engine
            auto* ceCh = m_curveEngine->channel(ch.topicId);
            if (ceCh && ceCh->count() > 0) {
                auto recent = ceCh->recentPoints(1);
                if (!recent.empty()) {
                    latest = recent[0].second;
                }
            }
        }
        QString text = QString("%1: %2").arg(ch.name).arg(latest, 6, 'f', 2);
        painter.drawText(x + 16, y + 10, text);

        x += 16 + painter.fontMetrics().horizontalAdvance(text) + 20;
        if (x > rect().right() - 100) {
            x = rect().left() + 70;
            y += 16;
        }
    }
}

void CurveWidget::drawAxisLabels(QPainter& painter, const QRect& rect)
{
    painter.setPen(QColor(100, 100, 110));
    painter.setFont(QFont("Consolas", 8));

    float range = m_yMax - m_yMin;
    if (range <= 0) range = 1;
    int hLines = 5;
    for (int i = 0; i <= hLines; ++i) {
        float val = m_yMax - range * i / hLines;
        int y = rect.top() + rect.height() * i / hLines;
        QString label = QString::number(val, 'f', 1);
        painter.drawText(rect.left() - 55, y + 4, 50, 12, Qt::AlignRight | Qt::AlignVCenter, label);
    }

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
// Mouse interaction
// ============================================================

void CurveWidget::resizeEvent(QResizeEvent* /*event*/)
{
    update();
}

void CurveWidget::wheelEvent(QWheelEvent* event)
{
    double factor = (event->angleDelta().y() > 0) ? 0.9 : 1.1;

    if (event->modifiers() & Qt::ControlModifier) {
        // Ctrl+Wheel: Y-axis value zoom (scale around cursor position)
        float yCenter = m_yMax - (float)event->pos().y() / height() * (m_yMax - m_yMin);
        float newRange = (m_yMax - m_yMin) * factor;
        float halfRange = newRange / 2.0f;
        m_yMin = yCenter - halfRange;
        m_yMax = yCenter + halfRange;
        m_autoScale = false;
    } else {
        // Wheel: X-axis time zoom
        m_xRangeSeconds = std::max(0.5, m_xRangeSeconds * factor);
    }

    update();
    notifyTimeAxisChange();
}

void CurveWidget::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        // WI-104: 左键框选缩放
        m_rubberBanding = true;
        m_rubberBandOrigin = event->pos();
        m_rubberBandRect = QRect(event->pos(), QSize(0, 0));
        setCursor(Qt::CrossCursor);
    } else if (event->button() == Qt::RightButton) {
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
        double dt = -delta.x() / (double)width() * m_xRangeSeconds;
        m_t0 = (m_t0 > dt * 1000000.0) ? static_cast<uint64_t>(m_t0 - dt * 1000000.0) : 0;
        update();
        notifyTimeAxisChange();
    } else if (m_rubberBanding) {
        // WI-104: 更新框选矩形
        m_rubberBandRect = QRect(m_rubberBandOrigin, event->pos()).normalized();
        update();
    }
}

void CurveWidget::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton && m_rubberBanding) {
        // WI-104: 完成框选缩放
        m_rubberBanding = false;
        setCursor(Qt::ArrowCursor);

        // 忽略过小的框选（可能是误点击）
        if (m_rubberBandRect.width() > 5 && m_rubberBandRect.height() > 5) {
            QRect plotRect = rect().adjusted(60, 20, -20, -40);

            // 将框选矩形的像素坐标转换为数据坐标
            QPointF dataTopLeft = pixelToData(m_rubberBandRect.topLeft(), plotRect);
            QPointF dataBottomRight = pixelToData(m_rubberBandRect.bottomRight(), plotRect);

            double tMin = std::min(dataTopLeft.x(), dataBottomRight.x());
            double tMax = std::max(dataTopLeft.x(), dataBottomRight.x());
            double vMin = std::min(dataTopLeft.y(), dataBottomRight.y());
            double vMax = std::max(dataTopLeft.y(), dataBottomRight.y());

            // 应用缩放
            m_t0 = static_cast<uint64_t>(m_t0 + tMin * 1000000.0);
            m_xRangeSeconds = std::max(0.1, tMax - tMin);
            m_yMin = static_cast<float>(vMin);
            m_yMax = static_cast<float>(vMax);
            m_autoScale = false;

            update();
            notifyTimeAxisChange();
        }
    } else if (event->button() == Qt::RightButton) {
        m_panning = false;
        setCursor(Qt::ArrowCursor);
    }
}

// ============================================================
// WI-104: 缩放交互
// ============================================================

void CurveWidget::mouseDoubleClickEvent(QMouseEvent* /*event*/)
{
    // 双击自动适应所有可见曲线
    autoFit();
}

void CurveWidget::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Escape && m_rubberBanding) {
        // Esc取消框选
        m_rubberBanding = false;
        m_rubberBandRect = QRect();
        setCursor(Qt::ArrowCursor);
        update();
    }
    QWidget::keyPressEvent(event);
}

void CurveWidget::zoomIn()
{
    // X轴放大20%（范围乘以0.8），Y轴放大20%
    m_xRangeSeconds = std::max(0.1, m_xRangeSeconds * 0.8);

    double yRange = m_yMax - m_yMin;
    double yCenter = (m_yMax + m_yMin) / 2.0;
    double newHalfRange = yRange * 0.4;  // 0.8 / 2
    m_yMin = static_cast<float>(yCenter - newHalfRange);
    m_yMax = static_cast<float>(yCenter + newHalfRange);

    m_autoScale = false;
    update();
    notifyTimeAxisChange();
}

void CurveWidget::zoomOut()
{
    // X轴缩小20%（范围乘以1.25），Y轴缩小20%
    m_xRangeSeconds = m_xRangeSeconds * 1.25;

    double yRange = m_yMax - m_yMin;
    double yCenter = (m_yMax + m_yMin) / 2.0;
    double newHalfRange = yRange * 0.625;  // 1.25 / 2
    m_yMin = static_cast<float>(yCenter - newHalfRange);
    m_yMax = static_cast<float>(yCenter + newHalfRange);

    m_autoScale = false;
    update();
    notifyTimeAxisChange();
}

void CurveWidget::autoFit()
{
    // 扫描所有可见通道数据范围
    if (m_channels.isEmpty()) {
        m_xRangeSeconds = 10.0;
        m_t0 = 0;
        m_yMin = -10;
        m_yMax = 10;
        m_autoScale = true;
        update();
        return;
    }

    uint64_t tMin = UINT64_MAX;
    uint64_t tMax = 0;
    float vMin = 1e9f;
    float vMax = -1e9f;
    bool hasData = false;

    for (const auto& ch : m_channels) {
        if (!ch.visible) continue;

        // CurveEngine模式：从引擎获取数据范围
        if (m_curveEngine && ch.topicId != 0) {
            auto* ceCh = m_curveEngine->channel(ch.topicId);
            if (ceCh && ceCh->count() > 0) {
                auto range = ceCh->dataRange();
                if (tMin == UINT64_MAX || range.minTime < tMin) tMin = range.minTime;
                if (range.maxTime > tMax) tMax = range.maxTime;
                if (range.minVal < vMin) vMin = range.minVal;
                if (range.maxVal > vMax) vMax = range.maxVal;
                hasData = true;
            }
        }

        // 传统push模式：从本地数据获取范围
        if (!ch.data.isEmpty()) {
            for (const auto& pt : ch.data) {
                uint64_t ts = static_cast<uint64_t>(pt.x() * 1000000.0) + m_t0;
                if (ts < tMin) tMin = ts;
                if (ts > tMax) tMax = ts;
            }
            if (ch.minVal < vMin) vMin = ch.minVal;
            if (ch.maxVal > vMax) vMax = ch.maxVal;
            hasData = true;
        }
    }

    if (!hasData) {
        m_xRangeSeconds = 10.0;
        m_t0 = 0;
        m_yMin = -10;
        m_yMax = 10;
        m_autoScale = true;
        update();
        return;
    }

    // X轴：设置为数据时间段
    if (tMax > tMin) {
        m_t0 = tMin;
        m_xRangeSeconds = (tMax - tMin) / 1000000.0;
        // 留5%边距
        m_xRangeSeconds *= 1.05;
    } else {
        m_t0 = tMin;
        m_xRangeSeconds = 1.0;
    }

    m_xRangeSeconds = std::max(0.1, m_xRangeSeconds);

    // Y轴：设置范围并加5%边距
    if (vMax > vMin) {
        float margin = (vMax - vMin) * 0.05f;
        m_yMin = vMin - margin;
        m_yMax = vMax + margin;
    } else {
        m_yMin = vMin - 1.0f;
        m_yMax = vMax + 1.0f;
    }

    m_autoScale = true;
    update();
    notifyTimeAxisChange();
}

void CurveWidget::resetView()
{
    // 重置为默认范围
    m_xRangeSeconds = 10.0;
    m_t0 = 0;
    m_yMin = 0;
    m_yMax = 100;
    m_autoScale = true;

    update();
    notifyTimeAxisChange();
}

} // namespace MotorStudio
