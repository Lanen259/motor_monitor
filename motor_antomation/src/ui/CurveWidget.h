#pragma once

#include <QWidget>
#include <QVector>
#include <QColor>
#include <QString>
#include <QPointF>
#include <QTimer>
#include <QElapsedTimer>
#include <cstdint>
#include <vector>
#include <utility>

namespace MotorStudio {

class CurveEngine;
class TimeAxisManager;

// Real-time curve rendering widget
// Supports direct push mode (legacy) and CurveEngine pull mode (P0 target)
class CurveWidget : public QWidget {
    Q_OBJECT
public:
    explicit CurveWidget(QWidget* parent = nullptr);

    // Channel management
    int addChannel(const QString& name, const QColor& color = Qt::cyan);
    void removeChannel(int index);
    void clearAllChannels();
    int channelCount() const { return m_channels.size(); }

    // Direct data push (legacy mode)
    void pushData(int channelIndex, float value, uint64_t timestampUs = 0);
    void pushFrame(const QVector<float>& values, uint64_t timestampUs = 0);

    // CurveEngine-backed mode (P0 target)
    // Set the engine and start timer-based pull at target FPS
    void attachCurveEngine(CurveEngine* engine, int fps = 30);
    void detachCurveEngine();

    // Externally-managed channel topic binding (for PlotCell selective channels)
    void setChannelTopicId(int index, uint32_t topicId);
    void setAutoPopulateChannels(bool enabled);

    // Display control
    void setYAxisLabel(const QString& label) { m_yAxisLabel = label; }
    void setXAxisLabel(const QString& label) { m_xAxisLabel = label; }
    void setAutoScale(bool enabled) { m_autoScale = enabled; }
    void setYRange(float min, float max);
    void setXRangeSeconds(double seconds);

    // Time-axis accessors (WI-008: grid sync)
    uint64_t timeBase() const { return m_t0; }
    double xRangeSeconds() const { return m_xRangeSeconds; }
    void setTimeBase(uint64_t t0);

    // TimeAxisManager integration (WI-103)
    void setTimeAxisManager(TimeAxisManager* manager);
    TimeAxisManager* timeAxisManager() const { return m_timeAxisManager; }
    void setTimeSynced(bool sync);
    bool isTimeSynced() const { return m_timeSynced; }

    // Toolbar zoom actions (WI-104)
    void zoomIn();
    void zoomOut();
    void autoFit();
    void resetView();

    // Channel color
    void setChannelColor(int index, const QColor& color);
    QColor channelColor(int index) const;

    // Channel accessors (WI-009: CurveManagerPanel support)
    QString channelName(int index) const;
    void setChannelVisible(int index, bool visible);
    bool isChannelVisible(int index) const;
    uint32_t channelTopicId(int index) const;

    // Clear data
    void clearData();

    // Save screenshot
    void saveScreenshot(const QString& filePath);

    // Frame timing (WI-010: high-frequency performance)
    double frameIntervalMs() const { return m_frameIntervalMs; }

signals:
    void timeAxisChanged();

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;

private slots:
    void onPullTimer();
    void onSharedRangeChanged(uint64_t t0, double xRangeSeconds);

private:
    struct Channel {
        QString name;
        QColor color;
        QVector<QPointF> data;  // (timestamp_seconds, value)
        bool visible = true;
        float minVal = 0;
        float maxVal = 0;
        uint32_t topicId = 0;   // bound CurveEngine topic (0 = none)
    };

    void updateAutoScale();
    void drawGrid(QPainter& painter, const QRect& rect);
    void drawCurves(QPainter& painter, const QRect& rect);
    void drawLegend(QPainter& painter);
    void drawAxisLabels(QPainter& painter, const QRect& rect);

    QPointF dataToPixel(const QPointF& dataPoint, const QRect& rect) const;
    QPointF pixelToData(const QPointF& pixel, const QRect& rect) const;

    // Time axis change notification (WI-103: delegates to manager or emits signal)
    void notifyTimeAxisChange();

    QVector<Channel> m_channels;
    QString m_yAxisLabel;
    QString m_xAxisLabel;
    bool m_autoScale;
    float m_yMin;
    float m_yMax;
    double m_xRangeSeconds;
    uint64_t m_t0;

    // Mouse interaction
    bool m_dragging;
    QPoint m_lastMousePos;
    bool m_panning;

    // Rubber-band zoom (WI-104)
    bool m_rubberBanding = false;
    QPoint m_rubberBandOrigin;
    QRect m_rubberBandRect;

    // CurveEngine pull mode
    CurveEngine* m_curveEngine = nullptr;
    QTimer* m_pullTimer = nullptr;
    bool m_autoPopulateChannels = true;  // true = legacy auto-sync, false = externally managed

    // Frame timing (WI-010)
    QElapsedTimer m_frameTimer;
    double m_frameIntervalMs = 0.0;

    // TimeAxisManager integration (WI-103)
    TimeAxisManager* m_timeAxisManager = nullptr;
    bool m_timeSynced = true;
    bool m_updatingFromManager = false;
};

} // namespace MotorStudio
