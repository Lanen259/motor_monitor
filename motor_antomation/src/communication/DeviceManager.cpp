#include "DeviceManager.h"

namespace MotorStudio {

struct DeviceManager::Impl {
    std::vector<std::unique_ptr<ITransport>> transports;
};

DeviceManager& DeviceManager::instance() {
    static DeviceManager mgr;
    return mgr;
}

DeviceManager::DeviceManager() : d(std::make_unique<Impl>()) {}
DeviceManager::~DeviceManager() = default;

std::vector<DeviceInfo> DeviceManager::discoverDevices() {
    // TODO: 扫描串口、CAN、TCP设备
    return {};
}

bool DeviceManager::connectDevice(const std::string& deviceId) {
    // TODO: 创建对应传输层并连接
    return false;
}

void DeviceManager::disconnectDevice(const std::string& deviceId) {
    // TODO: 断开并清理
}

void DeviceManager::disconnectAll() {
    // TODO: 断开所有设备
}

ITransport* DeviceManager::getTransport(const std::string& deviceId) {
    // TODO: 查找传输层
    return nullptr;
}

size_t DeviceManager::connectedCount() const {
    return 0;
}

} // namespace MotorStudio