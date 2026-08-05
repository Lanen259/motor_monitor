#pragma once
#include "ITransport.h"
#include <memory>

namespace MotorStudio {

class UdpTransport : public ITransport {
    Q_OBJECT
public:
    explicit UdpTransport(QObject* parent = nullptr);
    ~UdpTransport() override;

    bool open(const std::string& config) override;
    void close() override;
    bool isOpen() const override;
    bool send(const QByteArray& data) override;
    std::string transportType() const override { return "UDP"; }

private:
    struct Impl;
    std::unique_ptr<Impl> d;
};

} // namespace MotorStudio