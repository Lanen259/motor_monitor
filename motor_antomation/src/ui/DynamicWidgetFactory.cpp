#include "DynamicWidgetFactory.h"
#include <QPushButton>
#include <QSlider>
#include <QLineEdit>
#include <QCursor>

namespace MotorStudio {

QWidget* DynamicWidgetFactory::createWidget(const QString& type, QWidget* parent)
{
    QString t = type.trimmed().toLower();

    if (t == "button") {
        QPushButton* btn = new QPushButton("Button", parent);
        btn->setMinimumHeight(34);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setStyleSheet(
            "QPushButton {"
            "  background-color: #FFFFFF; color: #212121;"
            "  border: 1px solid #E0E0E0; border-radius: 4px;"
            "  padding: 6px 16px; font-size: 13px;"
            "}"
            "QPushButton:hover { background-color: #E3F2FD; border-color: #2196F3; }"
            "QPushButton:pressed { background-color: #2196F3; color: #FFFFFF; }"
        );
        return btn;
    }

    if (t == "slider") {
        QSlider* slider = new QSlider(Qt::Horizontal, parent);
        slider->setRange(0, 100);
        slider->setValue(50);
        slider->setMinimumHeight(30);
        slider->setStyleSheet(
            "QSlider::groove:horizontal {"
            "  height: 6px; background: #E0E0E0; border-radius: 3px;"
            "}"
            "QSlider::handle:horizontal {"
            "  width: 16px; margin: -6px 0; background: #2196F3;"
            "  border-radius: 8px;"
            "}"
            "QSlider::sub-page:horizontal {"
            "  background: #2196F3; border-radius: 3px;"
            "}"
        );
        return slider;
    }

    if (t == "input") {
        QLineEdit* input = new QLineEdit(parent);
        input->setPlaceholderText("Enter value...");
        input->setMinimumHeight(30);
        input->setStyleSheet(
            "QLineEdit {"
            "  background-color: #FAFAFA; color: #212121;"
            "  border: 1px solid #E0E0E0; border-radius: 4px;"
            "  padding: 4px 8px; font-size: 13px;"
            "}"
            "QLineEdit:focus { border-color: #2196F3; }"
        );
        return input;
    }

    return nullptr;
}

} // namespace MotorStudio
