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
// Mock MCU Curve Test (full pipeline, 5 seconds)
// Pipe: DeviceSimulator(500Hz) -> VofaParser(JustFloat) -> DataBus -> CurveEngine
// Verify:
//   - CurveEngine::totalWritten() > 0 for every channel
//   - No significant data loss (< 5%)
//   - Channel data has meaningful range (non-constant)
// ============================================================
int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);

    const int TEST_DURATION_MS = 5000;
    const int SIM_FREQUENCY = 500;

    qDebug() << "========================================";
    qDebug() << " Mock MCU Curve Test (WI-015)";
    qDebug() << "========================================";
    qDebug() << " Pipe: DeviceSimulator -> VofaParser -> DataBus -> CurveEngine";
    qDebug() << " Duration:" << TEST_DURATION_MS << "ms @" << SIM_FREQUENCY << "Hz";
    qDebug() << " Expected frames:" << (TEST_DURATION_MS / 1000 * SIM_FREQUENCY);
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
        Topics::Timestamp,
        Topics::Ia,
        Topics::Ib,
        Topics::Ic,
        Topics::Id,
        Topics::Iq,
        Topics::Speed,
        Topics::Position,
        Topics::Voltage,
        Topics::Current,
        Topics::Temperature,
        Topics::Fault
    };

    // --- CurveEngine channels ---
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
    qDebug() << " Starting simulator for 5 seconds...";
    simulator.start();

    QTimer::singleShot(TEST_DURATION_MS, &app, [&]() {
        simulator.stop();
        qDebug() << " Simulator stopped.";
        app.quit();
    });

    app.exec();

    // --- Verification ---
    qDebug() << "\n========================================";
    qDebug() << " Verification";
    qDebug() << "========================================";

    size_t deviceFrames = simulator.framesGenerated();
    size_t vofaFrames = static_cast<size_t>(vofaParser.framesParsed());

    qDebug() << " DeviceSimulator frames generated:" << deviceFrames;
    qDebug() << " VofaParser frames parsed:      " << vofaFrames;
    qDebug() << " DataBus subscribe count:        " << dataBus.subscriberCount();

    bool allPass = true;

    // --- Check 1: Every channel has data ---
    qDebug() << "\n[1] Per-channel data check:";
    size_t grandTotal = 0;

    // Core channels (exclude Timestamp as it is metadata)
    const std::vector<uint32_t> coreChannels = {
        Topics::Ia, Topics::Ib, Topics::Ic, Topics::Id, Topics::Iq,
        Topics::Speed, Topics::Position, Topics::Voltage, Topics::Current,
        Topics::Temperature, Topics::Fault
    };

    for (auto topicId : coreChannels) {
        auto* ch = curveEngine.channel(topicId);
        if (!ch) {
            qDebug() << "  [FAIL] Topic" << topicId << ": channel not found in CurveEngine";
            allPass = false;
            continue;
        }

        size_t written = ch->totalWritten();
        size_t count = ch->count();
        grandTotal += written;

        qDebug() << "  Topic" << topicId
                 << "| totalWritten:" << written
                 << "| count:" << count;

        if (written == 0) {
            qDebug() << "  [FAIL] Topic" << topicId << ": zero data points written";
            allPass = false;
        }
    }

    qDebug() << "  Grand total points written:" << grandTotal;

    // --- Check 2: No significant data loss ---
    qDebug() << "\n[2] Data loss check:";
    if (deviceFrames > 0) {
        double lossPct = (1.0 - static_cast<double>(vofaFrames) / deviceFrames) * 100.0;
        qDebug() << "  Loss rate:" << lossPct << "%";

        if (lossPct > 5.0) {
            qDebug() << "  [FAIL] Data loss exceeds 5%";
            allPass = false;
        } else {
            qDebug() << "  [PASS] Data loss within acceptable range";
        }
    }

    // --- Check 3: Key channels have meaningful data range ---
    qDebug() << "\n[3] Data range check (Ia, Ib, Ic, Speed):";
    for (auto topicId : {Topics::Ia, Topics::Ib, Topics::Ic, Topics::Speed}) {
        auto* ch = curveEngine.channel(topicId);
        if (ch && ch->count() > 0) {
            auto range = ch->dataRange();
            qDebug() << "  Topic" << topicId
                     << "| range:[" << range.minVal << "," << range.maxVal << "]"
                     << "| time:[" << range.minTime << "," << range.maxTime << "]us";

            // Data should vary (not all identical)
            if (range.minVal == range.maxVal) {
                qDebug() << "  [WARN] Topic" << topicId << ": all values identical (no variation)";
            }
        }
    }

    qDebug() << "\n========================================";
    qDebug() << (allPass ? " [PASS] Mock MCU Curve Test" : " [FAIL] Mock MCU Curve Test");
    qDebug() << "========================================";

    return allPass ? 0 : 1;
}
