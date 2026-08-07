#include "ChannelManager.h"
#include <QDateTime>
#include <QDebug>
#include <algorithm>

namespace MotorStudio {

// ============================================================
// ChannelRingBuffer
// ============================================================

ChannelRingBuffer::ChannelRingBuffer(const QString& name, int maxSeconds)
    : m_name(name)
    , m_maxSeconds(maxSeconds)
    , m_writeIndex(0)
    , m_count(0)
{
    // 预分配：假设 1kHz 数据，15分钟 = 900,000 点
    // 使用 1M 容量，足够覆盖极端情况
    m_buffer.reserve(1024 * 1024);
}

void ChannelRingBuffer::push(float value, uint64_t timestampUs)
{
    ChannelDataPoint dp(value, timestampUs);

    if (m_count < m_buffer.capacity()) {
        m_buffer.append(dp);
        m_count++;
    } else {
        m_buffer[m_writeIndex] = dp;
        m_writeIndex = (m_writeIndex + 1) % m_buffer.size();
    }

    // 每1000次推入执行一次旧数据清理
    static int trimCounter = 0;
    if (++trimCounter >= 1000) {
        trimCounter = 0;
        trimOldData();
    }
}

void ChannelRingBuffer::clear()
{
    m_buffer.clear();
    m_writeIndex = 0;
    m_count = 0;
}

ChannelDataPoint ChannelRingBuffer::latest() const
{
    if (m_count == 0) return ChannelDataPoint();
    if (m_count <= m_buffer.size()) {
        return m_buffer.last();
    }
    int idx = (m_writeIndex - 1 + m_buffer.size()) % m_buffer.size();
    return m_buffer[idx];
}

float ChannelRingBuffer::latestValue() const
{
    return latest().value;
}

void ChannelRingBuffer::setMaxSeconds(int seconds)
{
    if (seconds > 0) {
        m_maxSeconds = seconds;
        trimOldData();
    }
}

void ChannelRingBuffer::trimOldData()
{
    if (m_count == 0 || m_maxSeconds <= 0) return;

    uint64_t cutoffUs = 0;
    // 获取最新时间戳
    ChannelDataPoint last = latest();
    if (last.timestampUs > 0) {
        cutoffUs = last.timestampUs - static_cast<uint64_t>(m_maxSeconds) * 1000000ULL;
    }

    if (cutoffUs == 0) return;

    // 删除过期数据
    int removeCount = 0;
    while (removeCount < m_count) {
        const ChannelDataPoint& dp = m_buffer[removeCount];
        if (dp.timestampUs >= cutoffUs) break;
        removeCount++;
    }

    if (removeCount > 0 && removeCount < m_count) {
        m_buffer.remove(0, removeCount);
        m_count -= removeCount;
        m_writeIndex = std::max(0, m_writeIndex - removeCount);
    }
}

QVector<ChannelDataPoint> ChannelRingBuffer::rangeData(uint64_t startUs, uint64_t endUs) const
{
    QVector<ChannelDataPoint> result;
    for (const auto& dp : m_buffer) {
        if (dp.timestampUs >= startUs && dp.timestampUs <= endUs) {
            result.append(dp);
        }
    }
    return result;
}

// ============================================================
// ChannelManager
// ============================================================

ChannelManager::ChannelManager(QObject* parent)
    : QObject(parent)
{
}

ChannelManager::~ChannelManager()
{
    qDeleteAll(m_channels);
}

int ChannelManager::addChannel(const QString& name)
{
    auto* buf = new ChannelRingBuffer(name);
    int index = m_channels.size();
    m_channels.append(buf);
    emit channelAdded(index, name);
    return index;
}

void ChannelManager::removeChannel(int index)
{
    if (index < 0 || index >= m_channels.size()) return;
    QString name = m_channels[index]->name();
    delete m_channels[index];
    m_channels.removeAt(index);
    emit channelRemoved(index, name);
}

void ChannelManager::clearAll()
{
    for (auto* ch : m_channels) {
        ch->clear();
    }
    emit allCleared();
}

ChannelRingBuffer* ChannelManager::channel(int index)
{
    if (index < 0 || index >= m_channels.size()) return nullptr;
    return m_channels[index];
}

const ChannelRingBuffer* ChannelManager::channel(int index) const
{
    if (index < 0 || index >= m_channels.size()) return nullptr;
    return m_channels[index];
}

ChannelRingBuffer* ChannelManager::channelByName(const QString& name)
{
    for (auto* ch : m_channels) {
        if (ch->name() == name) return ch;
    }
    return nullptr;
}

QStringList ChannelManager::channelNames() const
{
    QStringList names;
    for (const auto* ch : m_channels) {
        names.append(ch->name());
    }
    return names;
}

void ChannelManager::pushFrame(const QVector<float>& values, uint64_t timestampUs)
{
    if (timestampUs == 0) {
        timestampUs = QDateTime::currentMSecsSinceEpoch() * 1000ULL;
    }

    // 确保通道数足够
    while (m_channels.size() < values.size()) {
        addChannel(QString("CH%1").arg(m_channels.size() + 1));
    }

    for (int i = 0; i < values.size() && i < m_channels.size(); ++i) {
        m_channels[i]->push(values[i], timestampUs);
    }

    emit framePushed(values);
}

float ChannelManager::latestValue(int index) const
{
    const auto* ch = channel(index);
    return ch ? ch->latestValue() : 0.0f;
}

QVector<float> ChannelManager::latestValues() const
{
    QVector<float> values;
    for (const auto* ch : m_channels) {
        values.append(ch->latestValue());
    }
    return values;
}

QVector<ChannelDataPoint> ChannelManager::channelData(int index, int offset, int count) const
{
    const auto* ch = channel(index);
    if (!ch) return {};

    const auto& data = ch->data();
    int total = data.size();
    if (offset >= total) return {};

    int end = (count < 0) ? total : std::min(offset + count, total);
    QVector<ChannelDataPoint> result;
    result.reserve(end - offset);
    for (int i = offset; i < end; ++i) {
        result.append(data[i]);
    }
    return result;
}

int ChannelManager::totalPoints() const
{
    int total = 0;
    for (const auto* ch : m_channels) {
        total += ch->count();
    }
    return total;
}

} // namespace MotorStudio