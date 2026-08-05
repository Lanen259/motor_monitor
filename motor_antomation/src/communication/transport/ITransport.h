#pragma once
#include <QObject>
#include <QByteArray>
#include <functional>
#include <memory>
#include <string>

namespace MotorStudio {

// 传输层抽象接口
class ITransport : public QObject {
    Q_OBJECT
public:
    explicit ITransport(QObject* parent = nullptr) : QObject(parent) {}
    ~ITransport() override = default;

    // 连接/断开
    virtual bool open(const std::string& config) = 0;
    virtual void close() = 0;
    virtual bool isOpen() const = 0;

    // 发送数据（异步）
    virtual bool send(const QByteArray& data) = 0;

    // 传输类型标识
    virtual std::string transportType() const = 0;

signals:
    void dataReceived(const QByteArray& data);
    void connected();
    void disconnected();
    void errorOccurred(const std::string& errorMsg);
};

} // namespace MotorStudio