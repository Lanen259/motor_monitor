#include "DeviceWorker.h"
#include "transport/SerialTransport.h"
#include "protocol/VofaParser.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QDebug>

DeviceWorker::DeviceWorker(QObject* parent)
    : QObject(parent)
{
    // Create transport and parser — they will live in whichever thread
    // this worker is moved to via moveToThread()
    m_transport = new MotorStudio::SerialTransport(this);
    m_parser = new MotorStudio::VofaParser(this);

    // Wire: raw data → protocol parser
    QObject::connect(m_transport, &MotorStudio::SerialTransport::dataReceived,
                     m_parser, &MotorStudio::VofaParser::feed);

    // Wire: parsed frame → signal to UI thread
    QObject::connect(m_parser, &MotorStudio::VofaParser::frameParsed,
                     this, &DeviceWorker::frameReady);

    // Wire: transport state → signals to UI thread
    QObject::connect(m_transport, &MotorStudio::SerialTransport::connected,
                     this, &DeviceWorker::deviceConnected);
    QObject::connect(m_transport, &MotorStudio::SerialTransport::disconnected,
                     this, &DeviceWorker::deviceDisconnected);
    QObject::connect(m_transport, &MotorStudio::SerialTransport::errorOccurred,
                     this, [this](const std::string& msg) {
        emit deviceError(QString::fromStdString(msg));
    });
}

DeviceWorker::~DeviceWorker()
{
    if (m_transport->isOpen()) {
        m_transport->close();
    }
}

void DeviceWorker::openDevice(const QString& configJson)
{
    QJsonDocument doc = QJsonDocument::fromJson(configJson.toUtf8());
    if (!doc.isObject()) {
        emit deviceError("Invalid config JSON");
        return;
    }

    QJsonObject cfg = doc.object();
    QString port = cfg["port"].toString();
    int baud = cfg["baud"].toInt(256000);

    // Configure VofaParser
    m_parser->setProtocolType(MotorStudio::VofaParser::JustFloat);

    // Build transport config
    QJsonObject transportCfg;
    transportCfg["port"] = port;
    transportCfg["baud"] = baud;

    std::string configStr = QJsonDocument(transportCfg).toJson(QJsonDocument::Compact).toStdString();
    m_transport->open(configStr);
}

void DeviceWorker::closeDevice()
{
    m_transport->close();
}
