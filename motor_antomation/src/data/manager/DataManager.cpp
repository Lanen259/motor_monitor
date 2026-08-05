#include "DataManager.h"
#include <QByteArray>
#include <algorithm>
#include <mutex>

namespace MotorStudio {

DataManager::DataManager(QObject* parent)
    : QObject(parent)
{
    snapshotCache_.reserve(maxCacheSize_);
    statsStartTime_ = std::chrono::steady_clock::now();
}

DataManager::~DataManager() {
    detachTransport();
}

void DataManager::attachTransport(ITransport* transport) {
    if (transport_) {
        detachTransport();
    }

    transport_ = transport;

    if (transport_) {
        connect(transport_, &ITransport::dataReceived,
                this, &DataManager::onRawDataReceived);
        connect(transport_, &ITransport::connected,
                this, &DataManager::onTransportConnected);
        connect(transport_, &ITransport::disconnected,
                this, &DataManager::onTransportDisconnected);
    }
}

void DataManager::detachTransport() {
    if (transport_) {
        disconnect(transport_, &ITransport::dataReceived,
                   this, &DataManager::onRawDataReceived);
        disconnect(transport_, &ITransport::connected,
                   this, &DataManager::onTransportConnected);
        disconnect(transport_, &ITransport::disconnected,
                   this, &DataManager::onTransportDisconnected);
        transport_ = nullptr;
    }
}

void DataManager::onTransportConnected() {
    emit transportConnected();
}

void DataManager::onTransportDisconnected() {
    emit transportDisconnected();
}

void DataManager::onRawDataReceived(const QByteArray& data) {
    framesReceived_++;

    // 喂入协议解析器
    std::vector<uint8_t> rawBytes(data.begin(), data.end());
    protocol_.feed(rawBytes);

    // 处理所有完整帧
    while (protocol_.hasFrame()) {
        auto frame = protocol_.popFrame();
        processFrame(frame);
    }
}

void DataManager::processFrame(const MotorProtocol::DecodedFrame& frame) {
    if (!frame.crcValid) {
        framesFailed_++;
        emit frameError("CRC check failed");
        return;
    }

    if (frame.command == MotorCommand::DataUpload) {
        MotorDataPayload payload;
        if (MotorProtocol::parseDataPayload(frame.payload, payload)) {
            framesParsed_++;

            // 转换为 MotorSnapshot
            MotorSnapshot snapshot;
            snapshot.timestampUs = static_cast<uint64_t>(payload.timestamp) * 1000; // ms -> us
            snapshot.ia = payload.ia;
            snapshot.ib = payload.ib;
            snapshot.ic = payload.ic;
            snapshot.id = payload.id;
            snapshot.iq = payload.iq;
            snapshot.speed = payload.speed;
            snapshot.position = payload.position;
            snapshot.busVoltage = payload.busVoltage;
            snapshot.busCurrent = payload.busCurrent;
            snapshot.temperature = payload.temperature;
            snapshot.faultCode = payload.fault;
            snapshot.state = (payload.fault != 0) ? MotorState::Fault : MotorState::Running;

            // 更新最新快照
            {
                std::lock_guard<std::mutex> lock(latestMutex_);
                latestSnapshot_ = snapshot;
                hasLatest_ = true;
            }

            // 加入缓存
            appendToCache(snapshot);

            // 广播信号
            emit snapshotUpdated(snapshot);
        } else {
            framesFailed_++;
            emit frameError("Failed to parse data payload");
        }
    }
    // 其他命令类型（Ack、Nack 等）暂不处理
}

void DataManager::appendToCache(const MotorSnapshot& snapshot) {
    std::lock_guard<std::mutex> lock(cacheMutex_);

    if (snapshotCache_.size() < maxCacheSize_) {
        snapshotCache_.push_back(snapshot);
    } else {
        // 环形覆盖
        snapshotCache_[cacheWriteIndex_] = snapshot;
        cacheWriteIndex_ = (cacheWriteIndex_ + 1) % maxCacheSize_;
    }
}

MotorSnapshot DataManager::latestSnapshot() const {
    std::lock_guard<std::mutex> lock(latestMutex_);
    return latestSnapshot_;
}

std::optional<MotorSnapshot> DataManager::latestSnapshotOpt() const {
    std::lock_guard<std::mutex> lock(latestMutex_);
    if (hasLatest_) {
        return latestSnapshot_;
    }
    return std::nullopt;
}

std::vector<MotorSnapshot> DataManager::recentSnapshots(size_t n) const {
    std::lock_guard<std::mutex> lock(cacheMutex_);

    if (snapshotCache_.empty()) return {};

    if (snapshotCache_.size() < maxCacheSize_) {
        // 线性增长阶段
        size_t start = (snapshotCache_.size() > n) ? (snapshotCache_.size() - n) : 0;
        return std::vector<MotorSnapshot>(snapshotCache_.begin() + start, snapshotCache_.end());
    } else {
        // 环形缓冲区阶段
        std::vector<MotorSnapshot> result;
        result.reserve(n);
        size_t count = std::min(n, maxCacheSize_);
        for (size_t i = 0; i < count; ++i) {
            size_t idx = (cacheWriteIndex_ + maxCacheSize_ - count + i) % maxCacheSize_;
            result.push_back(snapshotCache_[idx]);
        }
        return result;
    }
}

std::vector<MotorSnapshot> DataManager::queryRange(uint64_t startUs, uint64_t endUs) const {
    std::lock_guard<std::mutex> lock(cacheMutex_);

    std::vector<MotorSnapshot> result;
    for (const auto& snap : snapshotCache_) {
        if (snap.timestampUs >= startUs && snap.timestampUs <= endUs) {
            result.push_back(snap);
        }
    }
    return result;
}

void DataManager::setMaxCacheSize(size_t maxSnapshots) {
    std::lock_guard<std::mutex> lock(cacheMutex_);
    maxCacheSize_ = maxSnapshots;
    if (snapshotCache_.size() > maxCacheSize_) {
        snapshotCache_.resize(maxCacheSize_);
    }
    snapshotCache_.reserve(maxCacheSize_);
    cacheWriteIndex_ = 0;
}

size_t DataManager::cacheSize() const {
    std::lock_guard<std::mutex> lock(cacheMutex_);
    return snapshotCache_.size();
}

void DataManager::clearCache() {
    std::lock_guard<std::mutex> lock(cacheMutex_);
    snapshotCache_.clear();
    cacheWriteIndex_ = 0;
}

double DataManager::dataRate() const {
    auto now = std::chrono::steady_clock::now();
    double elapsed = std::chrono::duration<double>(now - statsStartTime_).count();
    if (elapsed <= 0.0) return 0.0;
    return static_cast<double>(framesParsed_) / elapsed;
}

} // namespace MotorStudio