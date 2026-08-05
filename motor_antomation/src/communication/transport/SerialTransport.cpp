#include "SerialTransport.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QSerialPortInfo>
#include <QDebug>

namespace MotorStudio {

SerialTransport::SerialTransport(QObject* parent)
    : ITransport(parent)
    , m_serial(new QSerialPort(this))
    , m_baudRate(256000)
{
    connect(m_serial, &QSerialPort::readyRead,
            this, &SerialTransport::onReadyRead);
    connect(m_serial, QOverload<QSerialPort::SerialPortError>::of(&QSerialPort::error),
            this, &SerialTransport::onErrorOccurred);
}

SerialTransport::~SerialTransport()
{
    close();
}

bool SerialTransport::parseConfig(const std::string& config, QString& port, qint32& baud)
{
    // 支持两种格式:
    // 1. JSON: {"port":"COM3","baud":256000}
    // 2. 简单格式: "COM3:256000"

    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(QByteArray::fromStdString(config), &err);

    if (err.error == QJsonParseError::NoError && doc.isObject()) {
        QJsonObject obj = doc.object();
        port = obj.value("port").toString();
        baud = obj.value("baud").toInt(256000);
        return !port.isEmpty();
    }

    // 简单格式: "COM3:256000"
    QString configStr = QString::fromStdString(config);
    QStringList parts = configStr.split(':');
    if (parts.size() >= 1) {
        port = parts[0].trimmed();
        if (parts.size() >= 2) {
            baud = parts[1].trimmed().toInt();
        }
        return !port.isEmpty();
    }

    return false;
}

bool SerialTransport::open(const std::string& config)
{
    if (m_serial->isOpen()) {
        close();
    }

    QString port;
    qint32 baud = 256000;
    if (!parseConfig(config, port, baud)) {
        emit errorOccurred("Invalid config format: " + config);
        return false;
    }

    m_serial->setPortName(port);
    m_serial->setBaudRate(baud);
    m_serial->setDataBits(QSerialPort::Data8);
    m_serial->setParity(QSerialPort::NoParity);
    m_serial->setStopBits(QSerialPort::OneStop);
    m_serial->setFlowControl(QSerialPort::NoFlowControl);

    if (!m_serial->open(QIODevice::ReadWrite)) {
        QString err = m_serial->errorString();
        emit errorOccurred(err.toStdString());
        return false;
    }

    m_serial->setReadBufferSize(65536); // 64KB 读缓冲

    m_portName = port;
    m_baudRate = baud;
    qDebug() << "[SerialTransport] Opened" << port << "@" << baud << "bps";
    emit connected();
    return true;
}

void SerialTransport::close()
{
    if (m_serial->isOpen()) {
        m_serial->close();
        qDebug() << "[SerialTransport] Closed" << m_portName;
    }
    m_portName.clear();
    m_readBuffer.clear();
    emit disconnected();
}

bool SerialTransport::isOpen() const
{
    return m_serial->isOpen();
}

bool SerialTransport::send(const QByteArray& data)
{
    if (!m_serial->isOpen()) {
        return false;
    }
    qint64 written = m_serial->write(data);
    if (written < 0) {
        emit errorOccurred(m_serial->errorString().toStdString());
        return false;
    }
    return written == data.size();
}

QStringList SerialTransport::availablePorts() const
{
    QStringList ports;
    const auto infos = QSerialPortInfo::availablePorts();
    for (const auto& info : infos) {
        ports.append(info.portName());
    }
    return ports;
}

QString SerialTransport::portName() const
{
    return m_portName;
}

qint32 SerialTransport::baudRate() const
{
    return m_baudRate;
}

QString SerialTransport::errorString() const
{
    return m_serial->errorString();
}

// === 私有槽 ===

void SerialTransport::onReadyRead()
{
    m_readBuffer.append(m_serial->readAll());
    // 发送原始数据给上层解析
    if (!m_readBuffer.isEmpty()) {
        emit dataReceived(m_readBuffer);
        m_readBuffer.clear();
    }
}

void SerialTransport::onErrorOccurred(QSerialPort::SerialPortError error)
{
    if (error == QSerialPort::NoError) return;

    QString msg;
    switch (error) {
    case QSerialPort::DeviceNotFoundError:
        msg = "Device not found";
        break;
    case QSerialPort::PermissionError:
        msg = "Permission denied";
        break;
    case QSerialPort::OpenError:
        msg = "Device already open";
        break;
    case QSerialPort::ResourceError:
        msg = "Device unexpectedly removed";
        close();
        break;
    case QSerialPort::TimeoutError:
        msg = "I/O timeout";
        break;
    default:
        msg = m_serial->errorString();
        break;
    }

    qDebug() << "[SerialTransport] Error:" << msg;
    emit errorOccurred(msg.toStdString());
}

} // namespace MotorStudio