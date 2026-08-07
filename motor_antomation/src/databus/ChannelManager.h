#pragma once

#include <QObject>
#include <QVector>
#include <QString>
#include <QStringList>
#include <cstdint>
#include <cmath>

namespace MotorStudio {

// ============================================================
// 带时间戳的通道数据点（内部使用，与 Message::DataPoint 不同）
// ============================================================
struct ChannelDataPoint {
    float value = 0.0f;
    uint64_t timestampUs = 0;

    ChannelDataPoint() = default;
    ChannelDataPoint(float v, uint64_t t) : value(v), timestampUs(t) {}
};

// ============================================================
// 15分钟环形缓冲区（单通道），非QObject，纯数据
// 自动循环覆盖，保留最近N秒的数据
// ============================================================
class ChannelRingBuffer {
public:
    explicit ChannelRingBuffer(const QString& name = QString(), int maxSeconds = 900);

    QString name() const { return m_name; }
    void setName(const QString& n) { m_name = n; }

    void push(float value, uint64_t timestampUs = 0);
    void clear();

    const QVector<ChannelDataPoint>& data() const { return m_buffer; }
    ChannelDataPoint latest() const;
    float latestValue() const;
    int count() const { return m_count; }

    int maxSeconds() const { return m_maxSeconds; }
    void setMaxSeconds(int seconds);

    QVector<ChannelDataPoint> rangeData(uint64_t startUs, uint64_t endUs) const;

private:
    void trimOldData();

    QString m_name;
    QVector<ChannelDataPoint> m_buffer;
    int m_maxSeconds;
    int m_writeIndex;
    int m_count;
};

// ============================================================
// 多通道管理器
// ============================================================
class ChannelManager : public QObject {
    Q_OBJECT
public:
    explicit ChannelManager(QObject* parent = nullptr);
    ~ChannelManager() override;

    int addChannel(const QString& name);
    void removeChannel(int index);
    void clearAll();

    int channelCount() const { return m_channels.size(); }
    ChannelRingBuffer* channel(int index);
    const ChannelRingBuffer* channel(int index) const;
    ChannelRingBuffer* channelByName(const QString& name);
    QStringList channelNames() const;

    // 批量数据推送（从 VofaParser 解析结果）
    void pushFrame(const QVector<float>& values, uint64_t timestampUs = 0);

    float latestValue(int index) const;
    QVector<float> latestValues() const;
    QVector<ChannelDataPoint> channelData(int index, int offset, int count) const;

    int totalPoints() const;

signals:
    void channelAdded(int index, const QString& name);
    void channelRemoved(int index, const QString& name);
    void framePushed(const QVector<float>& values);
    void allCleared();

private:
    QVector<ChannelRingBuffer*> m_channels;
};

} // namespace MotorStudio