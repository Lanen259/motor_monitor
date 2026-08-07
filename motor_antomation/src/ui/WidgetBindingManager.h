#pragma once
#include <QObject>
#include <QMap>
#include <QWidget>
#include <QString>

namespace MotorStudio {

// Manages command bindings for dynamic widgets.
// Each bound widget triggers a command string when its primary action fires:
//   QPushButton  -> clicked()
//   QSlider      -> valueChanged(int)
//   QLineEdit    -> returnPressed()
// The placeholder {value} in the command string is replaced with the widget's
// current value (button text, slider position, or line edit text).
class WidgetBindingManager : public QObject {
    Q_OBJECT
public:
    explicit WidgetBindingManager(QObject* parent = nullptr);
    ~WidgetBindingManager() override;

    void bind(QWidget* widget, const QString& command);
    void unbind(QWidget* widget);

    QString command(QWidget* widget) const;
    bool hasBinding(QWidget* widget) const;

signals:
    // Emitted with the resolved command string each time a bound widget fires.
    void commandExecuted(const QString& command);

private:
    QMap<QWidget*, QString> m_bindings;

    void setupConnections(QWidget* widget);
    void executeCommand(QWidget* widget);
};

} // namespace MotorStudio
