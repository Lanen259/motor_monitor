#pragma once

#include <QObject>
#include <QThread>
#include <QVector>
#include <QByteArray>
#include <memory>

namespace MotorStudio {
class SerialTransport;
class VofaParser;
}

// Worker object that lives on the communication thread.
// Owns SerialTransport + VofaParser, emits parsed frames to UI thread.
class DeviceWorker : public QObject {
    Q_OBJECT
public:
    explicit DeviceWorker(QObject* parent = nullptr);
    ~DeviceWorker() override;

    void setSerialTransport(MotorStudio::SerialTransport* transport);
    void setVofaParser(MotorStudio::VofaParser* parser);

public slots:
    // Called from UI thread to open/close connection
    void openDevice(const QString& configJson);
    void closeDevice();

signals:
    // Emitted to UI thread with parsed frame data
    void frameReady(const QVector<float>& values);
    void deviceConnected();
    void deviceDisconnected();
    void deviceError(const QString& message);

private:
    MotorStudio::SerialTransport* m_transport = nullptr;
    MotorStudio::VofaParser* m_parser = nullptr;
};
