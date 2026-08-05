#pragma once

#include <QWidget>
#include <QVector>
#include <QColor>
#include <QString>
#include <QPointF>
#include <QTimer>
#include <QElapsedTimer>
#include <cstdint>

namespace MotorStudio {

// ============================================================
// 实时曲线绘制控件
// 支持多通道、降采样显示、自动缩放、鼠标交互
// ============================================================
class CurveWidget : public QWidget {
    Q_OBJECT
public:
    explicit CurveWidget(QWidget* parent = nullptr);

    // 通道管理
    int addChannel(const QString& name, const QColor& color = Qt::cyan);
    void removeChannel(int index);
    void clearAllChannels();
    int channelCount() const { return m_channels.size(); }

    // 数据推送
    void pushData(int channelIndex, float value, uint64_t timestampUs = 0);
    void pushFrame(const QVector<float>& values, uint64_t timestampUs = 0);

    // 显示控制
    void setYAxisLabel(const QString& label) { m_yAxisLabel = label; }
    void setXAxisLabel(const QString& label) { m_xAxisLabel = label; }
    void setAutoScale(bool enabled) { m_autoScale = enabled; }
    void setYRange(float min, float max);
    void setXRangeSeconds(double seconds);

    // 通道颜色
    void setChannelColor(int index, const QColor& color);
    QColor channelColor(int index) const;

    // 清除数据
    void clearData();

    // 保存截图
    void saveScreenshot(const QString& filePath);

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

private:
    struct Channel {
        QString name;
        QColor color;
        QVector<QPointF> data;  // (timestamp, value)
        bool visible = true;
        float minVal = 0;
        float maxVal = 0;
    };

    void updateAutoScale();
    void drawGrid(QPainter& painter, const QRect& rect);
    void drawCurves(QPainter& painter, const QRect& rect);
    void drawLegend(QPainter& painter);
    void drawAxisLabels(QPainter& painter, const QRect& rect);

    QPointF dataToPixel(const QPointF& dataPoint, const QRect& rect) const;
    QPointF pixelToData(const QPointF& pixel, const QRect& rect) const;

    QVector<Channel> m_channels;
    QString m_yAxisLabel;
    QString m_xAxisLabel;
    bool m_autoScale;
    float m_yMin;
    float m_yMax;
    double m_xRangeSeconds;  // X轴显示范围（秒）
    uint64_t m_t0;           // 起始时间戳

    // 鼠标交互
    bool m_dragging;
    QPoint m_lastMousePos;
    bool m_panning;

    // 降采样
    static constexpr int kMaxDrawPoints = 2000;  // 每通道最多绘制点数
};

} // namespace MotorStudio