#pragma once
#include <QObject>
#include <QThread>
#include <QTimer>
#include <memory>
#include <atomic>
#include <chrono>
#include "../communication/protocol/MotorProtocol.h"
#include "../communication/transport/ITransport.h"

namespace MotorStudio {

// ============================================================
// 设备模拟器 —— 以 500Hz 频率模拟电机控制器产生实时数据
// ============================================================
class DeviceSimulator : public QObject {
    Q_OBJECT
public:
    explicit DeviceSimulator(QObject* parent = nullptr);
    ~DeviceSimulator() override;

    // --- 配置 ---
    void setDeviceId(uint8_t id) { deviceId_ = id; }
    uint8_t deviceId() const { return deviceId_; }

    void setFrequency(int hz) { frequency_ = hz; }
    int frequency() const { return frequency_; }

    void setTransport(ITransport* transport);

    // --- 电机参数配置 ---
    void setNominalSpeed(float rpm) { nominalSpeed_ = rpm; }
    void setNominalVoltage(float v) { nominalVoltage_ = v; }
    void setNominalCurrent(float a) { nominalCurrent_ = a; }
    void setPolePairs(int n) { polePairs_ = n; }

    // --- 故障模拟 ---
    void setFaultCode(uint16_t code) { faultCode_ = code; }
    void clearFault() { faultCode_ = 0; }

    // --- 控制 ---
    void start();
    void stop();
    bool isRunning() const { return running_; }

    // --- 统计 ---
    size_t framesGenerated() const { return framesGenerated_; }
    double elapsedSeconds() const;

signals:
    void started();
    void stopped();
    void dataGenerated(const MotorDataPayload& payload);
    void errorOccurred(const std::string& error);

private slots:
    void onTick();

private:
    uint8_t deviceId_ = 0x01;
    int frequency_ = 500;  // Hz
    ITransport* transport_ = nullptr;

    // 电机参数
    float nominalSpeed_ = 3000.0f;
    float nominalVoltage_ = 24.0f;
    float nominalCurrent_ = 10.0f;
    int polePairs_ = 4;

    // 故障码
    uint16_t faultCode_ = 0;

    // 状态
    std::atomic<bool> running_{false};
    std::atomic<size_t> framesGenerated_{0};
    std::chrono::steady_clock::time_point startTime_;

    // 定时器
    QTimer* timer_ = nullptr;

    // 协议编码器
    MotorProtocol protocol_;

    // 电机模型状态
    float simSpeed_ = 0.0f;
    float simPosition_ = 0.0f;
    float simId_ = 0.0f;
    float simIq_ = 0.0f;
    float simTime_ = 0.0f;

    // 生成一帧数据
    MotorDataPayload generateData();
};

} // namespace MotorStudio