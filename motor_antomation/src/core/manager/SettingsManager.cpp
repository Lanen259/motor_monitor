#include "SettingsManager.h"

namespace MotorStudio {

SettingsManager& SettingsManager::instance() {
    static SettingsManager mgr;
    return mgr;
}

SettingsManager::SettingsManager()
    : settings_(std::make_unique<QSettings>("MotorStudio", "MotorStudio")) {}

QVariant SettingsManager::value(const QString& key, const QVariant& defaultValue) const {
    return settings_->value(key, defaultValue);
}

void SettingsManager::setValue(const QString& key, const QVariant& value) {
    settings_->setValue(key, value);
    emit valueChanged(key, value);
}

void SettingsManager::load(const QString& filePath) {
    // TODO: 从指定文件加载
}

void SettingsManager::save(const QString& filePath) {
    settings_->sync();
}

} // namespace MotorStudio