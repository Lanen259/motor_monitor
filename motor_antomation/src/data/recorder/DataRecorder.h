#pragma once
#include <QObject>
#include <memory>
#include <string>
#include <vector>
#include "../../core/message/Message.h"

namespace MotorStudio {

// 数据记录器（将实时数据写入文件）
class DataRecorder : public QObject {
    Q_OBJECT
public:
    explicit DataRecorder(QObject* parent = nullptr);
    ~DataRecorder() override;

    // 开始/停止记录
    bool startRecording(const std::string& filePath, const std::vector<uint32_t>& topicIds);
    void stopRecording();
    bool isRecording() const;

    // 记录单个数据点
    void recordPoint(const DataPoint& point);

    // 获取文件大小
    uint64_t fileSize() const;

signals:
    void recordingStarted(const std::string& filePath);
    void recordingStopped();
    void recordingError(const std::string& error);

private:
    struct Impl;
    std::unique_ptr<Impl> d;
};

} // namespace MotorStudio