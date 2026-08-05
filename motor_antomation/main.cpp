#include "mainwindow.h"
#include "app/Application.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    a.setApplicationName("motor_antomation");
    a.setApplicationVersion("0.1.0");

    // 初始化核心模块（EventBus, DataBus, ModuleManager 等）
    MotorStudio::Application app;
    if (!app.initialize()) {
        return -1;
    }

    MainWindow w;
    w.show();

    int result = a.exec();
    app.shutdown();
    return result;
}