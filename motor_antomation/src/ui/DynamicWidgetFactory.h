#pragma once
#include <QWidget>
#include <QString>

namespace MotorStudio {

// Factory that creates interactive widgets based on type string.
// Supported types: "button" -> QPushButton, "slider" -> QSlider, "input" -> QLineEdit
class DynamicWidgetFactory {
public:
    static QWidget* createWidget(const QString& type, QWidget* parent = nullptr);
};

} // namespace MotorStudio
