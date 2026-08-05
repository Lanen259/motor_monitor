#pragma once
#include "ITransport.h"
#include <memory>

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

private:
    struct Impl;
    std::unique_ptr<Impl> d;
};

} // namespace MotorStudio