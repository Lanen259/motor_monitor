#include "DataRecorder.h"

namespace MotorStudio {

struct DataRecorder::Impl {
    bool recording = false;
};

DataRecorder::DataRecorder(QObject* parent) : QObject(parent), d(std::make_unique<Impl>()) {}
DataRecorder::~DataRecorder() = default;

bool DataRecorder::startRecording(const std::string& filePath, const std::vector<uint32_t>& topicIds) {
    d->recording = true;
    emit recordingStarted(filePath);
    return true;
}

void DataRecorder::stopRecording() {
    d->recording = false;
    emit recordingStopped();
}

bool DataRecorder::isRecording() const {
    return d->recording;
}

void DataRecorder::recordPoint(const DataPoint& point) {
    // TODO: 写入文件
}

uint64_t DataRecorder::fileSize() const {
    return 0;
}

} // namespace MotorStudio