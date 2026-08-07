#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QListWidget>
#include <QStackedWidget>
#include <QDockWidget>
#include <QTabWidget>
#include <QToolBar>
#include <QStatusBar>
#include <QLabel>
#include <QMenu>
#include <QAction>
#include <QComboBox>
#include <QPushButton>
#include <QPlainTextEdit>
#include <QTimer>
#include <QThread>
#include <QVBoxLayout>
#include <vector>
#include <cstdint>

namespace MotorStudio {
class ChannelManager;
class CurveWidget;
class MultiCurveContainer;
class DashboardWidget;
class FaultWidget;
class ParameterWidget;
class ParameterManager;
class CurveEngine;
class AutomationEngine;
class AutomationWidget;
class TestRunner;
class CurveManagerPanel;
}

class DeviceWorker;
namespace MotorStudio { class WidgetBindingManager; }

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

    QTabWidget *tabWidget() const { return m_tabWidget; }
    int addPage(const QString &title, QWidget *widget);
    void removePage(int index);

    static MainWindow* instance();
    static void log(const QString &message);

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
    void onChannelConfig();
    void onConnect();
    void onDisconnect();
    void refreshSerialPorts();
    void onExportCSV();
    void onSaveProject();
    void onLoadProject();

    // P0-03: Handlers for comm thread signals
    void onDeviceConnected();
    void onDeviceDisconnected();
    void onDeviceError(const QString& message);
    void onFrameReady(const QVector<float>& values);

    // WI-012: Dynamic widget slots
    void onAddDynamicWidget();
    void onDynamicWidgetContextMenu(const QPoint& pos);

private:
    void setupMenuBar();
    void setupToolBar();
    void setupStatusBar();
    void setupCentralWidget();
    void setupNavPanel();
    void setupPropertyDock();
    void setupLogConsole();
    void setupConnections();
    void setupCommThread();
    void setupDataPipeline();
    void setupAutomation();
    void setupDynamicWidgetDock();
    void createDefaultPages();

    // Menus
    QMenu *m_fileMenu;
    QMenu *m_viewMenu;
    QMenu *m_settingsMenu;
    QMenu *m_helpMenu;

    // Actions
    QAction *m_importProjectAction;
    QAction *m_exportProjectAction;
    QAction *m_exitAction;
    QAction *m_settingsAction;
    QAction *m_aboutAction;
    QAction *m_channelConfigAction;

    // Toolbar widgets
    QComboBox *m_portCombo;
    QComboBox *m_baudCombo;
    QPushButton *m_connectBtn;
    QPushButton *m_disconnectBtn;
    QPushButton *m_refreshBtn;
    QPushButton *m_addWidgetBtn;

    // Status bar
    QLabel *m_connectionStatus;
    QLabel *m_dataRate;

    // Central widget (deprecated, kept for API compat)
    QTabWidget *m_tabWidget;

    // Industrial layout widgets
    QListWidget *m_navList;
    QStackedWidget *m_workspaceStack;
    QStackedWidget *m_propertyStack;
    QDockWidget *m_propertyDock;
    QDockWidget *m_logDock;
    QPlainTextEdit *m_logOutput;
    QPushButton *m_logClearBtn;
    QPushButton *m_logAutoScrollBtn;

    // Dynamic widget system (WI-012)
    MotorStudio::WidgetBindingManager *m_widgetBindingManager;
    QDockWidget *m_dynamicWidgetDock;
    QWidget *m_dynamicWidgetContainer;
    QVBoxLayout *m_dynamicWidgetLayout;

    // Property panel labels for dynamic updates
    QLabel *m_propDashboardChannels;
    QLabel *m_propDashboardRate;
    QLabel *m_propOscilloscopeCurves;
    QLabel *m_propOscilloscopeFPS;
    QLabel *m_propSettingsParams;

    void refreshPropertyPanel();

    static MainWindow *s_instance;

    // Refresh timer
    QTimer *m_portRefreshTimer;

    // P0-03: Communication thread
    QThread* m_commThread = nullptr;
    DeviceWorker* m_deviceWorker = nullptr;

    // Data pipeline
    MotorStudio::ParameterManager *m_paramManager;
    MotorStudio::CurveEngine *m_curveEngine;
    MotorStudio::ChannelManager *m_channelManager;

    // Registered topic IDs
    std::vector<uint32_t> m_topicIds;
    uint64_t m_engineSubscriberId = 0;

    // UI components
    MotorStudio::MultiCurveContainer *m_curveContainer;
    MotorStudio::DashboardWidget *m_dashboardWidget;
    MotorStudio::FaultWidget *m_faultWidget;
    MotorStudio::ParameterWidget *m_paramWidget;

    // P3-01: Automation engine and UI
    MotorStudio::AutomationEngine *m_automationEngine = nullptr;
    MotorStudio::AutomationWidget *m_automationWidget = nullptr;
    MotorStudio::TestRunner *m_testRunner = nullptr;

    // WI-009: Curve manager panel
    MotorStudio::CurveManagerPanel *m_curveManagerPanel = nullptr;
};

#endif // MAINWINDOW_H
