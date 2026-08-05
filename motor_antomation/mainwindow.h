#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTabWidget>
#include <QToolBar>
#include <QStatusBar>
#include <QLabel>
#include <QMenu>
#include <QAction>
#include <QComboBox>
#include <QPushButton>
#include <QTimer>

namespace MotorStudio {
class SerialTransport;
class VofaParser;
class ChannelManager;
class CurveWidget;
class DashboardWidget;
class FaultWidget;
class ParameterWidget;
class ParameterManager;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

    // 页面管理
    QTabWidget *tabWidget() const { return m_tabWidget; }
    int addPage(const QString &title, QWidget *widget);
    void removePage(int index);

signals:
    void projectImportRequested(const QString &filePath);
    void projectExportRequested(const QString &filePath);
    void connectRequested(const QString &port, int baudRate);
    void disconnectRequested();

private slots:
    void onImportProject();
    void onExportProject();
    void onSettings();
    void onAbout();
    void onConnect();
    void onDisconnect();
    void refreshSerialPorts();
    void onExportCSV();
    void onSaveProject();
    void onLoadProject();

private:
    void setupMenuBar();
    void setupToolBar();
    void setupStatusBar();
    void setupCentralWidget();
    void setupConnections();
    void setupDataPipeline();
    void createDefaultPages();

    // 菜单
    QMenu *m_fileMenu;
    QMenu *m_viewMenu;
    QMenu *m_settingsMenu;
    QMenu *m_helpMenu;

    // 动作
    QAction *m_importProjectAction;
    QAction *m_exportProjectAction;
    QAction *m_exitAction;
    QAction *m_settingsAction;
    QAction *m_aboutAction;

    // 工具栏控件
    QComboBox *m_portCombo;
    QComboBox *m_baudCombo;
    QPushButton *m_connectBtn;
    QPushButton *m_disconnectBtn;
    QPushButton *m_refreshBtn;

    // 状态栏
    QLabel *m_connectionStatus;
    QLabel *m_dataRate;

    // 中央控件
    QTabWidget *m_tabWidget;

    // 刷新定时器
    QTimer *m_portRefreshTimer;

    // 串口传输
    MotorStudio::SerialTransport *m_serialTransport;
    MotorStudio::VofaParser *m_vofaParser;
    MotorStudio::ChannelManager *m_channelManager;
    MotorStudio::ParameterManager *m_paramManager;

    // UI 组件
    MotorStudio::CurveWidget *m_curveWidget;
    MotorStudio::DashboardWidget *m_dashboardWidget;
    MotorStudio::FaultWidget *m_faultWidget;
    MotorStudio::ParameterWidget *m_paramWidget;
};

#endif // MAINWINDOW_H