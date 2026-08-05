#pragma once
#include "ITransport.h"
#include <memory>

namespace MotorStudio {

class CanTransport : public ITransport {
    Q_OBJECT
public:
    explicit CanTransport(QObject* parent = nullptr);
    ~CanTransport() override;

    bool open(const std::string& config) override;
    void close() override;
    bool isOpen() const override;
    bool send(const QByteArray& data) override;
    std::string transportType() const override { return "CAN"; }

private:
    struct Impl;
    std::unique_ptr<Impl> d;
};

} // namespace MotorStudio