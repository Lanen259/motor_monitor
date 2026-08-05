#include "VofaParser.h"
#include <QDebug>
#include <cstring>

namespace MotorStudio {

VofaParser::VofaParser(QObject* parent)
    : QObject(parent)
    , m_protocolType(JustFloat)
    , m_channelCount(8)
    , m_separator('\n')
    , m_framesParsed(0)
    , m_errorsEncountered(0)
{
}

void VofaParser::setProtocolType(ProtocolType type)
{
    m_protocolType = type;
    reset();
}

void VofaParser::setChannelCount(int count)
{
    if (count > 0 && count <= 64) {
        m_channelCount = count;
    }
}

void VofaParser::setSeparator(char sep)
{
    m_separator = sep;
}

void VofaParser::feed(const QByteArray& data)
{
    if (data.isEmpty()) return;
    m_buffer.append(data);

    switch (m_protocolType) {
    case JustFloat:
        parseJustFloat();
        break;
    case FireWater:
        parseFireWater();
        break;
    }
}

void VofaParser::parseJustFloat()
{
    // JustFloat: 每 m_channelCount * 4 字节 = 一帧
    const int frameSize = m_channelCount * sizeof(float);

    while (m_buffer.size() >= frameSize) {
        QVector<float> frame;
        frame.reserve(m_channelCount);

        const char* ptr = m_buffer.constData();
        for (int i = 0; i < m_channelCount; ++i) {
            float value;
            std::memcpy(&value, ptr + i * sizeof(float), sizeof(float));
            frame.append(value);
        }

        m_buffer.remove(0, frameSize);
        emitFrame(frame);
    }
}

void VofaParser::parseFireWater()
{
    // FireWater: 以分隔符结尾的文本帧
    // 每帧格式: "1.23,4.56,7.89\n" 或 "1.23 4.56 7.89\n"

    while (true) {
        int sepIdx = m_buffer.indexOf(m_separator);
        if (sepIdx < 0) break;

        QByteArray line = m_buffer.left(sepIdx).trimmed();
        m_buffer.remove(0, sepIdx + 1);

        if (line.isEmpty()) continue;

        // 解析浮点数列表
        QVector<float> frame;
        QList<QByteArray> parts;

        // 尝试逗号分隔
        if (line.contains(',')) {
            parts = line.split(',');
        } else {
            parts = line.split(' ');
        }

        for (const auto& part : parts) {
            QByteArray trimmed = part.trimmed();
            if (trimmed.isEmpty()) continue;

            bool ok = false;
            float value = trimmed.toFloat(&ok);
            if (ok) {
                frame.append(value);
            }
        }

        if (!frame.isEmpty()) {
            emitFrame(frame);
        }
    }
}

void VofaParser::emitFrame(const QVector<float>& frame)
{
    m_framesParsed++;
    emit frameParsed(frame);
}

void VofaParser::reset()
{
    m_buffer.clear();
    m_framesParsed = 0;
    m_errorsEncountered = 0;
}

} // namespace MotorStudio