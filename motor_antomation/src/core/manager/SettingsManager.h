#pragma once
#include <QObject>
#include <QSettings>
#include <QVariant>
#include <memory>
#include <string>

namespace MotorStudio {

class SettingsManager : public QObject {
    Q_OBJECT
public:
    static SettingsManager& instance();

    QVariant value(const QString& key, const QVariant& defaultValue = {}) const;
    void setValue(const QString& key, const QVariant& value);

    void load(const QString& filePath);
    void save(const QString& filePath);

signals:
    void valueChanged(const QString& key, const QVariant& newValue);

private:
    SettingsManager();
    std::unique_ptr<QSettings> settings_;
};

} // namespace MotorStudio