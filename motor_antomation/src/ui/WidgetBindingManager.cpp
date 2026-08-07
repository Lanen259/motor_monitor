#include "WidgetBindingManager.h"
#include <QPushButton>
#include <QSlider>
#include <QLineEdit>

namespace MotorStudio {

WidgetBindingManager::WidgetBindingManager(QObject* parent)
    : QObject(parent)
{
}

WidgetBindingManager::~WidgetBindingManager()
{
    for (auto it = m_bindings.begin(); it != m_bindings.end(); ++it) {
        it.key()->disconnect(this);
    }
    m_bindings.clear();
}

void WidgetBindingManager::bind(QWidget* widget, const QString& command)
{
    if (!widget || command.isEmpty()) return;

    if (m_bindings.contains(widget)) {
        widget->disconnect(this);
    }

    m_bindings[widget] = command;
    setupConnections(widget);
}

void WidgetBindingManager::unbind(QWidget* widget)
{
    if (!widget || !m_bindings.contains(widget)) return;
    widget->disconnect(this);
    m_bindings.remove(widget);
}

QString WidgetBindingManager::command(QWidget* widget) const
{
    return m_bindings.value(widget, QString());
}

bool WidgetBindingManager::hasBinding(QWidget* widget) const
{
    return m_bindings.contains(widget);
}

void WidgetBindingManager::setupConnections(QWidget* widget)
{
    if (auto* btn = qobject_cast<QPushButton*>(widget)) {
        connect(btn, &QPushButton::clicked, this, [this, widget]() {
            executeCommand(widget);
        });
    } else if (auto* slider = qobject_cast<QSlider*>(widget)) {
        connect(slider, &QSlider::valueChanged, this, [this, widget](int) {
            executeCommand(widget);
        });
    } else if (auto* input = qobject_cast<QLineEdit*>(widget)) {
        connect(input, &QLineEdit::returnPressed, this, [this, widget]() {
            executeCommand(widget);
        });
    }
}

void WidgetBindingManager::executeCommand(QWidget* widget)
{
    if (!m_bindings.contains(widget)) return;

    QString cmd = m_bindings.value(widget);

    // Resolve {value} placeholder from the widget's current state
    QString valueStr;
    if (auto* btn = qobject_cast<QPushButton*>(widget)) {
        valueStr = btn->text();
    } else if (auto* slider = qobject_cast<QSlider*>(widget)) {
        valueStr = QString::number(slider->value());
    } else if (auto* input = qobject_cast<QLineEdit*>(widget)) {
        valueStr = input->text();
    }

    cmd.replace("{value}", valueStr);

    emit commandExecuted(cmd);
}

} // namespace MotorStudio
