#include <QtTest/QtTest>
#include "../../src/communication/protocol/VofaParser.h"

using namespace MotorStudio;

class TestVofaParser : public QObject {
    Q_OBJECT

private:
    VofaParser* m_parser = nullptr;
    QVector<QVector<float>> m_receivedFrames;

private slots:
    void initTestCase()
    {
        m_parser = new VofaParser();
        connect(m_parser, &VofaParser::frameParsed, this, [this](const QVector<float>& values) {
            m_receivedFrames.append(values);
        });
    }

    void cleanupTestCase()
    {
        delete m_parser;
    }

    void init()
    {
        m_receivedFrames.clear();
        m_parser->reset();
    }

    void testJustFloatDefaultChannelCount()
    {
        // Default channel count is 0 → no frames parsed
        m_parser->setProtocolType(VofaParser::JustFloat);
        float data[] = {1.0f, 2.0f, 3.0f};
        QByteArray raw(reinterpret_cast<const char*>(data), sizeof(data));
        m_parser->feed(raw);
        // With channelCount=0, no frames are emitted
        QCOMPARE(m_receivedFrames.size(), 0);
    }

    void testJustFloatSingleChannel()
    {
        m_parser->setProtocolType(VofaParser::JustFloat);
        m_parser->setChannelCount(1);

        float value = 3.14f;
        QByteArray raw(reinterpret_cast<const char*>(&value), sizeof(value));
        m_parser->feed(raw);

        QCOMPARE(m_receivedFrames.size(), 1);
        QCOMPARE(m_receivedFrames[0].size(), 1);
        QCOMPARE(m_receivedFrames[0][0], 3.14f);
    }

    void testJustFloatMultiChannel()
    {
        m_parser->setProtocolType(VofaParser::JustFloat);
        m_parser->setChannelCount(3);

        float data[] = {1.0f, 2.0f, 3.0f};
        QByteArray raw(reinterpret_cast<const char*>(data), sizeof(data));
        m_parser->feed(raw);

        QCOMPARE(m_receivedFrames.size(), 1);
        QCOMPARE(m_receivedFrames[0].size(), 3);
        QCOMPARE(m_receivedFrames[0][0], 1.0f);
        QCOMPARE(m_receivedFrames[0][1], 2.0f);
        QCOMPARE(m_receivedFrames[0][2], 3.0f);
    }

    void testJustFloatMultipleFrames()
    {
        m_parser->setProtocolType(VofaParser::JustFloat);
        m_parser->setChannelCount(2);

        float data[] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f};
        QByteArray raw(reinterpret_cast<const char*>(data), sizeof(data));
        m_parser->feed(raw);

        QCOMPARE(m_receivedFrames.size(), 3);
        QCOMPARE(m_receivedFrames[0][0], 1.0f);
        QCOMPARE(m_receivedFrames[0][1], 2.0f);
        QCOMPARE(m_receivedFrames[1][0], 3.0f);
        QCOMPARE(m_receivedFrames[1][1], 4.0f);
        QCOMPARE(m_receivedFrames[2][0], 5.0f);
        QCOMPARE(m_receivedFrames[2][1], 6.0f);
    }

    void testJustFloatPartialFrame()
    {
        // Feeding partial data (not enough for a complete frame) should buffer
        m_parser->setProtocolType(VofaParser::JustFloat);
        m_parser->setChannelCount(4);

        // Send only 2 floats (half a frame)
        float partial[] = {1.0f, 2.0f};
        QByteArray raw(reinterpret_cast<const char*>(partial), sizeof(partial));
        m_parser->feed(raw);

        QCOMPARE(m_receivedFrames.size(), 0);  // No complete frame yet

        // Send remaining 2 floats
        float remaining[] = {3.0f, 4.0f};
        QByteArray raw2(reinterpret_cast<const char*>(remaining), sizeof(remaining));
        m_parser->feed(raw2);

        QCOMPARE(m_receivedFrames.size(), 1);
        QCOMPARE(m_receivedFrames[0].size(), 4);
        QCOMPARE(m_receivedFrames[0][0], 1.0f);
        QCOMPARE(m_receivedFrames[0][3], 4.0f);
    }

    void testFireWaterSingleFrame()
    {
        m_parser->setProtocolType(VofaParser::FireWater);
        m_parser->setSeparator('\n');

        // FireWater: comma-separated values, newline-terminated
        QByteArray raw = QByteArray("1.0,2.0,3.0,4.0\n");
        m_parser->feed(raw);

        QVERIFY(m_receivedFrames.size() >= 1);
        QCOMPARE(m_receivedFrames[0].size(), 4);
        QCOMPARE(m_receivedFrames[0][0], 1.0f);
        QCOMPARE(m_receivedFrames[0][3], 4.0f);
    }

    void testReset()
    {
        m_parser->setProtocolType(VofaParser::JustFloat);
        m_parser->setChannelCount(2);

        float data[] = {1.0f, 2.0f};
        QByteArray raw(reinterpret_cast<const char*>(data), sizeof(data));
        m_parser->feed(raw);
        QCOMPARE(m_receivedFrames.size(), 1);

        m_parser->reset();
        QCOMPARE(m_parser->framesParsed(), 0);
        QCOMPARE(m_parser->errorsEncountered(), 0);
    }

    void testStatistics()
    {
        m_parser->setProtocolType(VofaParser::JustFloat);
        m_parser->setChannelCount(1);

        for (int i = 0; i < 10; ++i) {
            float v = static_cast<float>(i);
            QByteArray raw(reinterpret_cast<const char*>(&v), sizeof(v));
            m_parser->feed(raw);
        }

        QCOMPARE(m_parser->framesParsed(), 10);
    }
};

QTEST_MAIN(TestVofaParser)
#include "test_vofa_parser.moc"
