#pragma once

#include <QObject>
#include <QByteArray>
#include <QVector>
#include <functional>

namespace MotorStudio {

// ============================================================
// VOFA+ 协议解析器
// 支持 JustFloat（原始浮点）和 FireWater（分隔符帧）协议
// ============================================================
class VofaParser : public QObject {
    Q_OBJECT
public:
    enum ProtocolType {
        JustFloat,   // 每4字节一个float，无帧标记
        FireWater    // 分隔符分隔的帧，默认'\n'结尾
    };

    explicit VofaParser(QObject* parent = nullptr);

    // 协议配置
    void setProtocolType(ProtocolType type);
    ProtocolType protocolType() const { return m_protocolType; }

    // 通道数配置（JustFloat 需要知道每帧几个通道）
    void setChannelCount(int count);
    int channelCount() const { return m_channelCount; }

    // FireWater 分隔符
    void setSeparator(char sep);
    char separator() const { return m_separator; }

    // 喂入原始数据
    void feed(const QByteArray& data);

    // 重置解析器状态
    void reset();

    // 统计
    int framesParsed() const { return m_framesParsed; }
    int errorsEncountered() const { return m_errorsEncountered; }

signals:
    // 解析出一帧数据（所有通道的float值）
    void frameParsed(const QVector<float>& values);

    // 解析错误
    void parseError(const QString& message);

private:
    void parseJustFloat();
    void parseFireWater();
    void emitFrame(const QVector<float>& frame);

    ProtocolType m_protocolType;
    int m_channelCount;
    char m_separator;
    QByteArray m_buffer;
    int m_framesParsed;
    int m_errorsEncountered;
};

} // namespace MotorStudio