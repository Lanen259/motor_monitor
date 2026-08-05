#pragma once
#include "ITransport.h"
#include <memory>

namespace MotorStudio {

class TcpTransport : public ITransport {
    Q_OBJECT
public:
    explicit TcpTransport(QObject* parent = nullptr);
    ~TcpTransport() override;

    bool open(const std::string& config) override;
    void close() override;
    bool isOpen() const override;
    bool send(const QByteArray& data) override;
    std::string transportType() const override { return "TCP"; }

private:
    struct Impl;
    std::unique_ptr<Impl> d;
};

} // namespace MotorStudio