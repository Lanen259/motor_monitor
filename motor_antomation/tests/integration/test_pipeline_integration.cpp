#include <QCoreApplication>
#include <QTimer>
#include <QDebug>
#include <iostream>
#include <vector>

#include "../src/device/DeviceSimulator.h"
#include "../src/communication/protocol/VofaParser.h"
#include "../src/databus/DataBus.h"
#include "../src/databus/Topic.h"
#include "../src/curve/CurveEngine.h"

using namespace MotorStudio;

// ============================================================
// Serialize MotorDataPayload as 12-channel JustFloat binary frame
// ============================================================
static QByteArray serializeJustFloat(const MotorDataPayload& p)
{
    float fields[12];
    fields[0]  = static_cast<float>(p.timestamp);   // TopicId: Timestamp (12)
    fields[1]  = p.ia;                               // TopicId: Ia (1)
    fields[2]  = p.ib;                               // TopicId: Ib (2)
    fields[3]  = p.ic;                               // TopicId: Ic (3)
    fields[4]  = p.id;                               // TopicId: Id (4)
    fields[5]  = p.iq;                               // TopicId: Iq (5)
    fields[6]  = p.speed;                            // TopicId: Speed (6)
    fields[7]  = p.position;                         // TopicId: Position (7)
    fields[8]  = p.busVoltage;                       // TopicId: Voltage (8)
    fields[9]  = p.busCurrent;                       // TopicId: Current (9)
    fields[10] = p.temperature;                      // TopicId: Temperature (10)
    fields[11] = static_cast<float>(p.fault);        // TopicId: Fault (11)
    return QByteArray(reinterpret_cast<const char*>(fields), static_cast<int>(sizeof(fields)));
}

// ============================================================
// Pipeline Integration Test
// Pipe: DeviceSimulator(500Hz) -> VofaParser(JustFloat) -> DataBus -> CurveEngine
// Verify: >=100 data points per channel after 2+ seconds
// ============================================================
int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);

    const int TEST_DURATION_MS = 2500;
    const int SIM_FREQUENCY = 500;
    const int EXPECTED_MIN_POINTS = 100;

    qDebug() << "========================================";
    qDebug() << " Pipeline Integration Test (WI-015)";
    qDebug() << "========================================";
    qDebug() << " Pipe: DeviceSimulator -> VofaParser(JustFloat) -> DataBus -> CurveEngine";
    qDebug() << " Duration:" << TEST_DURATION_MS << "ms @" << SIM_FREQUENCY << "Hz";
    qDebug() << " Minimum expected points per channel:" << EXPECTED_MIN_POINTS;
    qDebug() << "----------------------------------------";

    // --- Components ---
    DeviceSimulator simulator;
    VofaParser vofaParser;
    CurveEngine curveEngine;

    // --- Config: DeviceSimulator ---
    simulator.setDeviceId(0x01);
    simulator.setFrequency(SIM_FREQUENCY);
    simulator.setNominalSpeed(3000.0f);
    simulator.setNominalVoltage(24.0f);

    // --- Config: VofaParser (12-channel JustFloat) ---
    vofaParser.setProtocolType(VofaParser::JustFloat);
    vofaParser.setChannelCount(12);

    // --- Channel topic IDs (must match serializeJustFloat order) ---
    const std::vector<uint32_t> channelTopics = {
        Topics::Timestamp,  //  0: float(timestamp)
        Topics::Ia,         //  1
        Topics::Ib,         //  2
        Topics::Ic,         //  3
        Topics::Id,         //  4
        Topics::Iq,         //  5
        Topics::Speed,      //  6
        Topics::Position,   //  7
        Topics::Voltage,    //  8
        Topics::Current,    //  9
        Topics::Temperature,// 10
        Topics::Fault       // 11
    };

    // --- Config: CurveEngine channels ---
    for (auto topicId : channelTopics) {
        curveEngine.addChannel(topicId, 100000);
    }

    // --- Wire: DeviceSimulator -> VofaParser ---
    QObject::connect(&simulator, &DeviceSimulator::dataGenerated,
                     [&vofaParser](const MotorDataPayload& payload) {
                         QByteArray raw = serializeJustFloat(payload);
                         vofaParser.feed(raw);
                     });

    // --- Wire: VofaParser -> DataBus ---
    auto& dataBus = DataBus::instance();
    QObject::connect(&vofaParser, &VofaParser::frameParsed,
                     [&dataBus, &channelTopics](const QVector<float>& values) {
                         // Index 0 is timestamp (ms), convert to us
                         uint64_t tsUs = static_cast<uint64_t>(values[0]) * 1000ULL;
                         dataBus.publishFrame(channelTopics, values, tsUs);
                     });

    // --- Wire: DataBus -> CurveEngine ---
    for (auto topicId : channelTopics) {
        dataBus.subscribe(topicId, [&curveEngine](const DataPoint& point) {
            curveEngine.append(point);
        });
    }

    // --- Run ---
    qDebug() << " Starting simulator...";
    simulator.start();

    QTimer::singleShot(TEST_DURATION_MS, &app, [&]() {
        simulator.stop();
        qDebug() << " Simulator stopped.";
        app.quit();
    });

    app.exec();

    // --- Verify ---
    qDebug() << "\n========================================";
    qDebug() << " Verification";
    qDebug() << "========================================";
    qDebug() << " Frames generated (DeviceSimulator):" << simulator.framesGenerated();
    qDebug() << " Frames parsed    (VofaParser):    " << vofaParser.framesParsed();

    bool allPass = true;

    for (auto topicId : channelTopics) {
        auto* ch = curveEngine.channel(topicId);
        if (!ch) {
            qDebug() << " [FAIL] Topic" << topicId << ": channel not found in CurveEngine";
            allPass = false;
            continue;
        }

        size_t count = ch->count();
        size_t totalW = ch->totalWritten();

        qDebug() << " Topic" << topicId
                 << "| count:" << count
                 << "| totalWritten:" << totalW;

        if (count < static_cast<size_t>(EXPECTED_MIN_POINTS)) {
            qDebug() << " [FAIL] Topic" << topicId << ": only" << count
                     << "points, expected at least" << EXPECTED_MIN_POINTS;
            allPass = false;
        }
    }

    // Sanity: VofaParser should have parsed a reasonable number of frames
    if (vofaParser.framesParsed() < EXPECTED_MIN_POINTS) {
        qDebug() << " [FAIL] VofaParser frame count too low:" << vofaParser.framesParsed();
        allPass = false;
    }

    qDebug() << "\n========================================";
    qDebug() << (allPass ? " [PASS] Pipeline Integration Test" : " [FAIL] Pipeline Integration Test");
    qDebug() << "========================================";

    return allPass ? 0 : 1;
}
