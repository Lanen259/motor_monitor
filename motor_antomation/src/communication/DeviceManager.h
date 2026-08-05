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
    // configJson 为 JSON 字符串，格式: {"port":"COM3","baudRate":256000,...}
    bool connectDevice(const std::string& deviceId, const std::string& configJson);
    void disconnectDevice(const std::string& deviceId);
    void disconnectAll();

    // 获取传输层
    ITransport* getTransport(const std::string& deviceId);

    // 查询设备信息
    DeviceInfo deviceInfo(const std::string& deviceId) const;
    bool isDeviceConnected(const std::string& deviceId) const;

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