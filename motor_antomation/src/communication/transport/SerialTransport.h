#pragma once
#include "ITransport.h"
#include <memory>
#include <QSerialPort>

namespace MotorStudio {

class SerialTransport : public ITransport {
    Q_OBJECT
public:
    explicit SerialTransport(QObject* parent = nullptr);
    ~SerialTransport() override;

    bool open(const std::string& config) override;
    void close() override;
    bool isOpen() const override;
    bool send(const QByteArray& data) override;
    std::string transportType() const override { return "Serial"; }

    // 串口特定接口
    QStringList availablePorts() const;
    QString portName() const;
    qint32 baudRate() const;
    QString errorString() const;

private slots:
    void onReadyRead();
    void onErrorOccurred(QSerialPort::SerialPortError error);

private:
    bool parseConfig(const std::string& config, QString& port, qint32& baud);

    QSerialPort* m_serial;
    QString m_portName;
    qint32 m_baudRate;
    QByteArray m_readBuffer;
};

} // namespace MotorStudio