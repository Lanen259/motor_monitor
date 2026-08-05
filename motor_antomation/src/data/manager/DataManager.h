#pragma once
#include <QObject>
#include <QThread>
#include <memory>
#include <atomic>
#include <mutex>
#include <optional>
#include <vector>
#include "../model/MotorData.h"
#include "../../communication/protocol/MotorProtocol.h"
#include "../../communication/transport/ITransport.h"

namespace MotorStudio {

// ============================================================
// 数据管理器 —— 接收原始帧、解析、缓存、广播
// ============================================================
class DataManager : public QObject {
    Q_OBJECT
public:
    explicit DataManager(QObject* parent = nullptr);
    ~DataManager() override;

    // --- 绑定传输层 ---
    void attachTransport(ITransport* transport);
    void detachTransport();

    // --- 获取最新快照 ---
    MotorSnapshot latestSnapshot() const;
    std::optional<MotorSnapshot> latestSnapshotOpt() const;

    // --- 获取历史快照 ---
    // 返回最近 N 个快照
    std::vector<MotorSnapshot> recentSnapshots(size_t n) const;

    // 按时间范围查询
    std::vector<MotorSnapshot> queryRange(uint64_t startUs, uint64_t endUs) const;

    // --- 缓存控制 ---
    void setMaxCacheSize(size_t maxSnapshots);
    size_t cacheSize() const;
    void clearCache();

    // --- 统计 ---
    size_t framesReceived() const { return framesReceived_; }
    size_t framesParsed() const { return framesParsed_; }
    size_t framesFailed() const { return framesFailed_; }
    size_t crcErrors() const { return protocol_.crcErrors(); }
    double dataRate() const;  // 帧/秒

signals:
    void snapshotUpdated(const MotorSnapshot& snapshot);
    void frameError(const std::string& error);
    void transportConnected();
    void transportDisconnected();

private slots:
    void onRawDataReceived(const QByteArray& data);
    void onTransportConnected();
    void onTransportDisconnected();

private:
    void processFrame(const MotorProtocol::DecodedFrame& frame);
    void appendToCache(const MotorSnapshot& snapshot);

    MotorProtocol protocol_;
    ITransport* transport_ = nullptr;

    // 环形缓存
    static constexpr size_t DEFAULT_MAX_CACHE = 60000; // 默认缓存 60秒 * 500Hz = 30000，取 60000
    mutable std::mutex cacheMutex_;
    std::vector<MotorSnapshot> snapshotCache_;
    size_t maxCacheSize_ = DEFAULT_MAX_CACHE;
    size_t cacheWriteIndex_ = 0;  // 环形写入位置

    // 最新快照
    mutable std::mutex latestMutex_;
    MotorSnapshot latestSnapshot_;
    bool hasLatest_ = false;

    // 统计
    std::atomic<size_t> framesReceived_{0};
    std::atomic<size_t> framesParsed_{0};
    std::atomic<size_t> framesFailed_{0};
    std::chrono::steady_clock::time_point statsStartTime_;
};

} // namespace MotorStudio