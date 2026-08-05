#pragma once
#include <QObject>
#include <memory>
#include <vector>
#include <string>
#include "transport/ITransport.h"

namespace MotorStudio {

// 设备描述
struct DeviceInfo {
    std::string deviceId;
    std::string name;
    std::string transportType;
    bool connected = false;
};

// 设备管理器
class DeviceManager : public QObject {
    Q_OBJECT
public:
    static DeviceManager& instance();
    ~DeviceManager();

    // 设备发现
    std::vector<DeviceInfo> discoverDevices();

    // 连接管理
    bool connectDevice(const std::string& deviceId);
    void disconnectDevice(const std::string& deviceId);
    void disconnectAll();

    // 获取传输层
    ITransport* getTransport(const std::string& deviceId);

    size_t connectedCount() const;

signals:
    void deviceConnected(const std::string& deviceId);
    void deviceDisconnected(const std::string& deviceId);
    void deviceError(const std::string& deviceId, const std::string& error);

private:
    DeviceManager();
    struct Impl;
    std::unique_ptr<Impl> d;
};

} // namespace MotorStudio