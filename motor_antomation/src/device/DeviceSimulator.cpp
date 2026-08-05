#include "DeviceSimulator.h"
#include <QTimer>
#include <QByteArray>
#include <cmath>
#include <random>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace MotorStudio {

DeviceSimulator::DeviceSimulator(QObject* parent)
    : QObject(parent)
{
    timer_ = new QTimer(this);
    timer_->setTimerType(Qt::PreciseTimer);
    connect(timer_, &QTimer::timeout, this, &DeviceSimulator::onTick);
}

DeviceSimulator::~DeviceSimulator() {
    stop();
}

void DeviceSimulator::setTransport(ITransport* transport) {
    transport_ = transport;
}

void DeviceSimulator::start() {
    if (running_) return;

    simSpeed_ = 0.0f;
    simPosition_ = 0.0f;
    simId_ = 0.0f;
    simIq_ = 0.0f;
    simTime_ = 0.0f;
    framesGenerated_ = 0;
    startTime_ = std::chrono::steady_clock::now();

    running_ = true;

    int intervalMs = 1000 / frequency_;
    timer_->start(intervalMs);

    emit started();
}

void DeviceSimulator::stop() {
    if (!running_) return;

    timer_->stop();
    running_ = false;
    emit stopped();
}

MotorDataPayload DeviceSimulator::generateData() {
    MotorDataPayload p;

    // 时间戳（毫秒）
    auto now = std::chrono::steady_clock::now();
    p.timestamp = static_cast<uint32_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(now - startTime_).count());

    // 模拟电机动态
    float dt = 1.0f / frequency_;
    simTime_ += dt;

    // 速度斜坡：0 -> 3000 RPM（5秒斜坡）
    float targetSpeed = nominalSpeed_;
    float rampTime = 5.0f;
    float rampFactor = std::min(1.0f, simTime_ / rampTime);

    // 添加一些速度波动（±2%）
    float speedNoise = 1.0f + 0.02f * std::sin(simTime_ * 10.0f);
    simSpeed_ = targetSpeed * rampFactor * speedNoise;

    // 位置：电角度积分
    float electricalSpeed = simSpeed_ * polePairs_ / 60.0f; // 电频率 (Hz)
    simPosition_ += electricalSpeed * 360.0f * dt;           // 电角度 (度)
    while (simPosition_ >= 360.0f) simPosition_ -= 360.0f;
    while (simPosition_ < 0.0f) simPosition_ += 360.0f;

    float posRad = simPosition_ * M_PI / 180.0f;

    // FOC 电流模型
    // id ≈ 0 (最优控制)
    simId_ = 0.05f * std::sin(simTime_ * 50.0f); // 微小波动

    // iq 与负载成正比
    float loadTorque = 0.5f + 0.3f * std::sin(simTime_ * 2.0f); // 变化负载
    simIq_ = nominalCurrent_ * 0.7f * rampFactor * loadTorque;

    // 添加电流纹波（PWM 频率相关）
    float rippleAmp = 0.02f * nominalCurrent_;

    // 反 Park + Clarke → ia, ib, ic
    float cosTheta = std::cos(posRad);
    float sinTheta = std::sin(posRad);

    float iAlpha = simId_ * cosTheta - simIq_ * sinTheta;
    float iBeta  = simId_ * sinTheta + simIq_ * cosTheta;

    p.ia = iAlpha + rippleAmp * std::sin(simTime_ * 2000.0f);
    p.ib = -0.5f * iAlpha + 0.866f * iBeta + rippleAmp * std::sin(simTime_ * 2000.0f + 2.094f);
    p.ic = -0.5f * iAlpha - 0.866f * iBeta + rippleAmp * std::sin(simTime_ * 2000.0f + 4.189f);

    p.id = simId_;
    p.iq = simIq_;
    p.speed = simSpeed_;
    p.position = simPosition_;

    // 母线电压：轻微波动
    p.busVoltage = nominalVoltage_ * (1.0f + 0.01f * std::sin(simTime_ * 3.0f));

    // 母线电流：与功率成正比
    float power = p.busVoltage * p.iq * 0.8f;
    p.busCurrent = power / nominalVoltage_ + 0.1f * std::sin(simTime_ * 5.0f);

    // 温度：缓慢上升
    p.temperature = 25.0f + 45.0f * rampFactor + 2.0f * std::sin(simTime_ * 0.5f);

    // 故障码
    p.fault = faultCode_;

    return p;
}

void DeviceSimulator::onTick() {
    if (!running_) return;

    // 生成数据
    MotorDataPayload payload = generateData();
    framesGenerated_++;

    // 发出信号
    emit dataGenerated(payload);

    // 编码并发送到传输层
    if (transport_ && transport_->isOpen()) {
        std::vector<uint8_t> frame = protocol_.encodeDataUpload(deviceId_, payload);
        QByteArray data(reinterpret_cast<const char*>(frame.data()),
                        static_cast<int>(frame.size()));
        transport_->send(data);
    }
}

double DeviceSimulator::elapsedSeconds() const {
    auto now = std::chrono::steady_clock::now();
    return std::chrono::duration<double>(now - startTime_).count();
}

} // namespace MotorStudio