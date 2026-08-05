#include "mainwindow.h"
#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    a.setApplicationName("Motor Automation");
    a.setApplicationVersion("0.1.0");
    a.setOrganizationName("MotorAutomation");

    MainWindow w;
    w.show();

    return a.exec();
}