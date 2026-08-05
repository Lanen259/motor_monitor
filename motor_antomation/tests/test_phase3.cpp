#include <QCoreApplication>
#include <QTimer>
#include <QDebug>
#include <iostream>
#include <iomanip>
#include <chrono>
#include <thread>
#include <atomic>
#include <mutex>
#include <deque>
#include <cmath>
#include <algorithm>
#include <numeric>

#include "../src/communication/transport/LoopbackTransport.h"
#include "../src/communication/protocol/MotorProtocol.h"
#include "../src/device/DeviceSimulator.h"
#include "../src/data/manager/DataManager.h"
#include "../src/databus/DataBus.h"
#include "../src/curve/CurveEngine.h"

using namespace MotorStudio;

// ============================================================
// 延迟测量器
// ============================================================
class LatencyTracker {
public:
    void recordSend(uint32_t timestampMs) {
        std::lock_guard<std::mutex> lock(mutex_);
        pending_[timestampMs] = std::chrono::steady_clock::now();
    }

    void recordReceive(uint32_t timestampMs) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = pending_.find(timestampMs);
        if (it != pending_.end()) {
            auto now = std::chrono::steady_clock::now();
            double latencyUs = std::chrono::duration<double, std::micro>(now - it->second).count();
            latencies_.push_back(latencyUs);
            pending_.erase(it);
        }
    }

    struct Stats {
        size_t count;
        double avgUs;
        double minUs;
        double maxUs;
        double p50Us;
        double p95Us;
        double p99Us;
    };

    Stats stats() const {
        std::lock_guard<std::mutex> lock(mutex_);
        Stats s;
        s.count = latencies_.size();
        if (latencies_.empty()) {
            s.avgUs = s.minUs = s.maxUs = s.p50Us = s.p95Us = s.p99Us = 0;
            return s;
        }

        auto sorted = latencies_;
        std::sort(sorted.begin(), sorted.end());

        s.minUs = sorted.front();
        s.maxUs = sorted.back();
        s.avgUs = std::accumulate(sorted.begin(), sorted.end(), 0.0) / sorted.size();

        auto percentile = [&](double p) {
            size_t idx = static_cast<size_t>(p / 100.0 * (sorted.size() - 1));
            return sorted[std::min(idx, sorted.size() - 1)];
        };

        s.p50Us = percentile(50);
        s.p95Us = percentile(95);
        s.p99Us = percentile(99);

        return s;
    }

    size_t pendingCount() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return pending_.size();
    }

private:
    mutable std::mutex mutex_;
    std::unordered_map<uint32_t, std::chrono::steady_clock::time_point> pending_;
    std::vector<double> latencies_;
};

// ============================================================
// 测试主程序
// ============================================================
int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);

    const int TEST_DURATION_SEC = 60;
    const int SIM_FREQUENCY = 500;
    const int EXPECTED_FRAMES = TEST_DURATION_SEC * SIM_FREQUENCY; // 30000

    qDebug() << "========================================";
    qDebug() << " Phase 3: 实时数据链路测试";
    qDebug() << "========================================";
    qDebug() << " 模拟频率:" << SIM_FREQUENCY << "Hz";
    qDebug() << " 测试时长:" << TEST_DURATION_SEC << "秒";
    qDebug() << " 预期帧数:" << EXPECTED_FRAMES;
    qDebug() << "----------------------------------------";

    // --- 创建组件 ---
    LoopbackTransport transport;
    DeviceSimulator simulator;
    DataManager dataManager;
    CurveEngine curveEngine;
    LatencyTracker latencyTracker;

    // --- 配置 ---
    transport.open("loopback://");
    simulator.setDeviceId(0x01);
    simulator.setFrequency(SIM_FREQUENCY);
    simulator.setNominalSpeed(3000.0f);
    simulator.setNominalVoltage(24.0f);
    simulator.setTransport(&transport);

    // --- 连接信号 ---
    // 设备模拟器 -> 延迟追踪（发送端）
    QObject::connect(&simulator, &DeviceSimulator::dataGenerated,
                     [&](const MotorDataPayload& payload) {
                         latencyTracker.recordSend(payload.timestamp);
                     });

    // 传输层 -> 数据管理器
    dataManager.attachTransport(&transport);

    // 数据管理器 -> 延迟追踪（接收端）+ 曲线引擎
    QObject::connect(&dataManager, &DataManager::snapshotUpdated,
                     [&](const MotorSnapshot& snapshot) {
                         // 记录延迟
                         uint32_t timestampMs = static_cast<uint32_t>(snapshot.timestampUs / 1000);
                         latencyTracker.recordReceive(timestampMs);

                         // 写入曲线引擎
                         uint64_t ts = snapshot.timestampUs;
                         curveEngine.append(Topics::Ia, ts, snapshot.ia);
                         curveEngine.append(Topics::Ib, ts, snapshot.ib);
                         curveEngine.append(Topics::Ic, ts, snapshot.ic);
                         curveEngine.append(Topics::Speed, ts, snapshot.speed);
                     });

    // --- 设置曲线通道 ---
    curveEngine.addChannel(Topics::Ia, 10000);
    curveEngine.addChannel(Topics::Ib, 10000);
    curveEngine.addChannel(Topics::Ic, 10000);
    curveEngine.addChannel(Topics::Speed, 10000);

    // --- 启动 ---
    qDebug() << " 启动模拟器...";
    simulator.start();

    // --- 运行 60 秒 ---
    qDebug() << " 测试运行中...";
    QTimer::singleShot(TEST_DURATION_SEC * 1000, &app, [&]() {
        simulator.stop();
        qDebug() << " 测试完成，正在统计...";
        app.quit();
    });

    app.exec();

    // --- 统计结果 ---
    qDebug() << "\n========================================";
    qDebug() << " 测试结果";
    qDebug() << "========================================";

    // 1. 数据丢失率
    size_t framesGenerated = simulator.framesGenerated();
    size_t framesReceived = dataManager.framesParsed();
    double lossRate = (framesGenerated > 0)
        ? (1.0 - static_cast<double>(framesReceived) / framesGenerated) * 100.0
        : 0.0;

    qDebug() << "\n[1] 数据完整性";
    qDebug() << "    模拟器生成帧数:" << framesGenerated;
    qDebug() << "    DataManager 解析帧数:" << framesReceived;
    qDebug() << "    传输层发送包数:" << transport.packetsSent();
    qDebug() << "    传输层接收包数:" << transport.packetsReceived();
    qDebug() << "    传输层丢包数:" << transport.packetsLost();
    qDebug() << "    CRC 错误数:" << dataManager.crcErrors();
    qDebug() << "    解析失败数:" << dataManager.framesFailed();
    qDebug() << QString("    数据丢失率: %1%").arg(lossRate, 0, 'f', 4);
    qDebug() << "    实际数据速率:" << dataManager.dataRate() << "fps";

    // 2. 线程状态
    qDebug() << "\n[2] 线程状态";
    qDebug() << "    模拟器运行状态:" << (simulator.isRunning() ? "运行中" : "已停止");
    qDebug() << "    传输层状态:" << (transport.isOpen() ? "已连接" : "已断开");
    qDebug() << "    DataBus 订阅者数:" << DataBus::instance().subscriberCount();
    qDebug() << "    DataBus 发布速率:" << DataBus::instance().publishRate() << "pps";

    // 3. 消息延迟
    qDebug() << "\n[3] 消息延迟 (us)";
    auto latStats = latencyTracker.stats();
    qDebug() << "    延迟样本数:" << latStats.count;
    qDebug() << QString("    平均延迟: %1 us").arg(latStats.avgUs, 0, 'f', 2);
    qDebug() << QString("    最小延迟: %1 us").arg(latStats.minUs, 0, 'f', 2);
    qDebug() << QString("    最大延迟: %1 us").arg(latStats.maxUs, 0, 'f', 2);
    qDebug() << QString("    P50 延迟: %1 us").arg(latStats.p50Us, 0, 'f', 2);
    qDebug() << QString("    P95 延迟: %1 us").arg(latStats.p95Us, 0, 'f', 2);
    qDebug() << QString("    P99 延迟: %1 us").arg(latStats.p99Us, 0, 'f', 2);
    qDebug() << "    未匹配延迟样本:" << latencyTracker.pendingCount();

    // 4. CurveEngine 缓存状态
    qDebug() << "\n[4] CurveEngine 缓存状态";
    for (uint32_t topicId : {Topics::Ia, Topics::Ib, Topics::Ic, Topics::Speed}) {
        auto* ch = curveEngine.channel(topicId);
        if (ch) {
            auto range = ch->dataRange();
            auto recent = ch->recentPoints(1);
            qDebug() << "    Topic" << topicId
                     << "| 缓存点数:" << ch->count() << "/" << ch->capacity()
                     << "| 总写入:" << ch->totalWritten()
                     << "| 范围:[" << range.minVal << "," << range.maxVal << "]";
            if (!recent.empty()) {
                qDebug() << "       最新值:" << recent[0].second
                         << " @ t=" << recent[0].first << "us";
            }
        }
    }

    // 5. 最新快照
    qDebug() << "\n[5] 最新数据快照";
    auto snap = dataManager.latestSnapshot();
    qDebug() << "    Timestamp:" << snap.timestampUs << "us";
    qDebug() << "    Ia:" << snap.ia << "A  Ib:" << snap.ib << "A  Ic:" << snap.ic << "A";
    qDebug() << "    Id:" << snap.id << "A  Iq:" << snap.iq << "A";
    qDebug() << "    Speed:" << snap.speed << "RPM  Position:" << snap.position << "°";
    qDebug() << "    BusVoltage:" << snap.busVoltage << "V  BusCurrent:" << snap.busCurrent << "A";
    qDebug() << "    Temperature:" << snap.temperature << "°C  Fault:" << snap.faultCode;
    qDebug() << "    State:" << static_cast<int>(snap.state);

    // 6. 综合评估
    qDebug() << "\n[6] 综合评估";
    bool pass = true;
    if (lossRate > 1.0) {
        qDebug() << "    [FAIL] 数据丢失率过高:" << lossRate << "%";
        pass = false;
    } else {
        qDebug() << "    [PASS] 数据丢失率:" << lossRate << "% (< 1%)";
    }
    if (latStats.avgUs > 5000.0) {
        qDebug() << "    [WARN] 平均延迟偏高:" << latStats.avgUs << "us";
    } else {
        qDebug() << "    [PASS] 平均延迟:" << latStats.avgUs << "us (< 5000us)";
    }
    if (framesReceived < EXPECTED_FRAMES * 0.99) {
        qDebug() << "    [WARN] 实际帧数偏低:" << framesReceived << "/" << EXPECTED_FRAMES;
    } else {
        qDebug() << "    [PASS] 实际帧数:" << framesReceived << "/" << EXPECTED_FRAMES;
    }

    qDebug() << "\n========================================";
    qDebug() << (pass ? " 测试通过" : " 测试未通过");
    qDebug() << "========================================";

    return pass ? 0 : 1;
}