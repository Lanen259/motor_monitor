#include "DeviceManager.h"
#include "transport/SerialTransport.h"

#include <QSerialPortInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDebug>
#include <map>
#include <algorithm>

namespace MotorStudio {

struct DeviceManager::Impl {
    struct TransportEntry {
        std::unique_ptr<ITransport> transport;
        DeviceInfo info;
    };

    std::map<std::string, TransportEntry> devices;

    // 扫描串口设备
    std::vector<DeviceInfo> discoverSerialDevices() {
        std::vector<DeviceInfo> result;
        const auto infos = QSerialPortInfo::availablePorts();
        for (const auto& info : infos) {
            DeviceInfo di;
            di.deviceId = info.portName().toStdString();
            di.name = (info.description().isEmpty()
                       ? info.portName()
                       : info.description() + " (" + info.portName() + ")")
                          .toStdString();
            di.transportType = "Serial";
            di.connected = false;
            result.push_back(std::move(di));
        }
        return result;
    }

    // 根据 deviceId 查找已连接设备
    TransportEntry* findDevice(const std::string& deviceId) {
        auto it = devices.find(deviceId);
        return (it != devices.end()) ? &it->second : nullptr;
    }

    const TransportEntry* findDevice(const std::string& deviceId) const {
        auto it = devices.find(deviceId);
        return (it != devices.end()) ? &it->second : nullptr;
    }
};

DeviceManager& DeviceManager::instance() {
    static DeviceManager mgr;
    return mgr;
}

DeviceManager::DeviceManager() : d(std::make_unique<Impl>()) {}

DeviceManager::~DeviceManager() {
    disconnectAll();
}

// ============================================================================
// 设备发现
// ============================================================================

std::vector<DeviceInfo> DeviceManager::discoverDevices() {
    std::vector<DeviceInfo> result = d->discoverSerialDevices();

    // 将已连接设备的 connected 状态标记为 true
    for (auto& di : result) {
        auto entry = d->findDevice(di.deviceId);
        if (entry && entry->transport && entry->transport->isOpen()) {
            di.connected = true;
        }
    }

    return result;
}

// ============================================================================
// 连接管理
// ============================================================================

bool DeviceManager::connectDevice(const std::string& deviceId,
                                  const std::string& configJson) {
    // 如果设备已连接，先断开
    if (d->findDevice(deviceId)) {
        disconnectDevice(deviceId);
    }

    // 创建 SerialTransport
    auto transport = std::make_unique<SerialTransport>();
    // 连接 transport 信号到 DeviceManager 信号
    QObject::connect(transport.get(), &ITransport::connected,
                     [this, deviceId]() {
        emit deviceConnected(deviceId);
    });
    QObject::connect(transport.get(), &ITransport::disconnected,
                     [this, deviceId]() {
        emit deviceDisconnected(deviceId);
    });
    QObject::connect(transport.get(), &ITransport::errorOccurred,
                     [this, deviceId](const std::string& error) {
        emit deviceError(deviceId, error);
    });

    // 尝试打开
    if (!transport->open(configJson)) {
        return false;
    }

    // 构建设备信息
    DeviceInfo info;
    info.deviceId = deviceId;
    info.transportType = transport->transportType();
    info.connected = true;

    // 从 JSON 配置中提取设备名称
    QJsonDocument doc = QJsonDocument::fromJson(
        QByteArray::fromStdString(configJson));
    if (doc.isObject()) {
        QJsonObject cfg = doc.object();
        QString portName = cfg["port"].toString();
        QString name = cfg["name"].toString();
        info.name = name.isEmpty() ? portName.toStdString() : name.toStdString();
    }
    if (info.name.empty()) {
        info.name = deviceId;
    }

    // 存入设备表
    Impl::TransportEntry entry;
    entry.transport = std::move(transport);
    entry.info = info;
    d->devices[deviceId] = std::move(entry);

    qDebug() << "[DeviceManager] Device connected:" << deviceId.c_str();
    return true;
}

void DeviceManager::disconnectDevice(const std::string& deviceId) {
    auto* entry = d->findDevice(deviceId);
    if (!entry || !entry->transport) {
        return;
    }

    // 断开
    entry->transport->close();
    d->devices.erase(deviceId);

    qDebug() << "[DeviceManager] Device disconnected:" << deviceId.c_str();
}

void DeviceManager::disconnectAll() {
    // 收集所有 deviceId（避免在迭代中修改 map）
    std::vector<std::string> ids;
    ids.reserve(d->devices.size());
    for (const auto& pair : d->devices) {
        ids.push_back(pair.first);
    }

    for (const auto& id : ids) {
        disconnectDevice(id);
    }
}

// ============================================================================
// 查询
// ============================================================================

ITransport* DeviceManager::getTransport(const std::string& deviceId) {
    auto* entry = d->findDevice(deviceId);
    return (entry && entry->transport) ? entry->transport.get() : nullptr;
}

DeviceInfo DeviceManager::deviceInfo(const std::string& deviceId) const {
    auto* entry = d->findDevice(deviceId);
    if (entry) {
        return entry->info;
    }

    DeviceInfo empty;
    empty.deviceId = deviceId;
    return empty;
}

bool DeviceManager::isDeviceConnected(const std::string& deviceId) const {
    auto* entry = d->findDevice(deviceId);
    return entry && entry->transport && entry->transport->isOpen();
}

size_t DeviceManager::connectedCount() const {
    return std::count_if(d->devices.begin(), d->devices.end(),
                         [](const auto& pair) {
        return pair.second.transport && pair.second.transport->isOpen();
    });
}

} // namespace MotorStudio