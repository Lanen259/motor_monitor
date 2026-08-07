#include "mainwindow.h"
#include "communication/DeviceWorker.h"
#include "databus/ChannelManager.h"
#include "databus/DataBus.h"
#include "databus/Topic.h"
#include "curve/CurveEngine.h"
#include "ui/CurveWidget.h"
#include "ui/MultiCurveContainer.h"
#include "ui/DashboardWidget.h"
#include "ui/FaultWidget.h"
#include "ui/ChannelConfigDialog.h"
#include "ui/AutomationWidget.h"
#include "ui/DynamicWidgetFactory.h"
#include "ui/WidgetBindingManager.h"
#include "ui/CurveManagerPanel.h"
#include "automation/AutomationEngine.h"
#include "automation/TestRunner.h"
#include "parameter/ParameterManager.h"

#include <QApplication>
#include <QMenuBar>
#include <QToolBar>
#include <QStatusBar>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QComboBox>
#include <QPushButton>
#include <QAction>
#include <QListWidget>
#include <QStackedWidget>
#include <QDockWidget>
#include <QPlainTextEdit>
#include <QFileDialog>
#include <QMessageBox>
#include <QSerialPortInfo>
#include <QTimer>
#include <QSettings>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDebug>
#include <QFile>
#include <QTextStream>
#include <QDateTime>
#include <QCloseEvent>
#include <QElapsedTimer>
#include <QTextCursor>
#include <QSplitter>
#include <QInputDialog>
#include <QDialogButtonBox>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_tabWidget(nullptr)
    , m_navList(nullptr)
    , m_workspaceStack(nullptr)
    , m_propertyStack(nullptr)
    , m_propertyDock(nullptr)
    , m_logDock(nullptr)
    , m_logOutput(nullptr)
    , m_logClearBtn(nullptr)
    , m_logAutoScrollBtn(nullptr)
    , m_propDashboardChannels(nullptr)
    , m_propDashboardRate(nullptr)
    , m_propOscilloscopeCurves(nullptr)
    , m_propOscilloscopeFPS(nullptr)
    , m_propSettingsParams(nullptr)
    , m_paramManager(new MotorStudio::ParameterManager(this))
    , m_curveEngine(new MotorStudio::CurveEngine(this))
    , m_channelManager(new MotorStudio::ChannelManager(this))
    , m_curveContainer(nullptr)
    , m_dashboardWidget(nullptr)
    , m_faultWidget(nullptr)
    , m_paramWidget(nullptr)
    , m_automationEngine(nullptr)
    , m_automationWidget(nullptr)
    , m_testRunner(nullptr)
    , m_widgetBindingManager(new MotorStudio::WidgetBindingManager(this))
    , m_dynamicWidgetDock(nullptr)
    , m_dynamicWidgetContainer(nullptr)
    , m_dynamicWidgetLayout(nullptr)
{
    s_instance = this;

    qApp->setStyleSheet(
        "QMainWindow { background-color: #F5F7FA; }"
        "QWidget { font-family: 'Microsoft YaHei', 'Segoe UI', sans-serif; }"
    );

    setWindowTitle("Motor Automation");
    resize(1200, 800);
    setMinimumSize(800, 600);

    setupMenuBar();
    setupToolBar();
    setupStatusBar();
    setupCentralWidget();
    setupCommThread();     // P0-03: Communication on dedicated thread
    setupDataPipeline();    // P0-02: DataBus-centric pipeline
    setupAutomation();      // P3-01: Automation engine
    setupConnections();
    createDefaultPages();

    refreshSerialPorts();

    m_portRefreshTimer = new QTimer(this);
    m_portRefreshTimer->setInterval(2000);
    connect(m_portRefreshTimer, &QTimer::timeout, this, &MainWindow::refreshSerialPorts);
    m_portRefreshTimer->start();

    QSettings settings;
    restoreGeometry(settings.value("MainWindow/geometry").toByteArray());
    restoreState(settings.value("MainWindow/state").toByteArray());
}

MainWindow::~MainWindow()
{
    // Graceful shutdown: stop automation test if running, move engine back to main thread
    if (m_automationEngine) {
        m_automationEngine->stop();
        // Move engine back to main thread before TestRunner's worker thread is destroyed
        m_automationEngine->moveToThread(QThread::currentThread());
    }

    // Stop test runner (stops worker thread)
    delete m_testRunner;
    m_testRunner = nullptr;

    // Safe to delete engine now that it's back on main thread
    delete m_automationEngine;
    m_automationEngine = nullptr;

    // Graceful shutdown: stop comm thread
    if (m_commThread && m_commThread->isRunning()) {
        m_commThread->quit();
        m_commThread->wait(3000);
    }

    QSettings settings;
    settings.setValue("MainWindow/geometry", saveGeometry());
    settings.setValue("MainWindow/state", saveState());
}

void MainWindow::setupMenuBar()
{
    m_fileMenu = menuBar()->addMenu(tr("File(&F)"));

    m_importProjectAction = new QAction(tr("Import Project..."), this);
    m_importProjectAction->setShortcut(QKeySequence("Ctrl+O"));
    m_fileMenu->addAction(m_importProjectAction);

    m_exportProjectAction = new QAction(tr("Export Project..."), this);
    m_exportProjectAction->setShortcut(QKeySequence("Ctrl+S"));
    m_fileMenu->addAction(m_exportProjectAction);

    m_fileMenu->addSeparator();

    m_exitAction = new QAction(tr("Exit(&X)"), this);
    m_exitAction->setShortcut(QKeySequence("Alt+F4"));
    m_fileMenu->addAction(m_exitAction);

    m_viewMenu = menuBar()->addMenu(tr("View(&V)"));
    m_viewMenu->addAction(tr("Default Layout"))->setEnabled(false);

    m_settingsMenu = menuBar()->addMenu(tr("Settings(&S)"));
    m_settingsAction = new QAction(tr("Preferences..."), this);
    m_settingsMenu->addAction(m_settingsAction);

    m_channelConfigAction = new QAction(tr("Channel Configuration..."), this);
    m_settingsMenu->addAction(m_channelConfigAction);

    m_helpMenu = menuBar()->addMenu(tr("Help(&H)"));
    m_aboutAction = new QAction(tr("About..."), this);
    m_helpMenu->addAction(m_aboutAction);
}

void MainWindow::setupToolBar()
{
    QToolBar *toolbar = addToolBar(tr("Main Toolbar"));
    toolbar->setMovable(false);
    toolbar->setIconSize(QSize(24, 24));

    toolbar->addWidget(new QLabel(tr("  Port: ")));
    m_portCombo = new QComboBox();
    m_portCombo->setMinimumWidth(120);
    m_portCombo->setToolTip(tr("Select serial port"));
    toolbar->addWidget(m_portCombo);

    toolbar->addWidget(new QLabel(tr(" Baud: ")));
    m_baudCombo = new QComboBox();
    m_baudCombo->setMinimumWidth(100);
    m_baudCombo->addItems({"9600", "19200", "38400", "57600", "115200", "256000", "460800", "921600", "1000000", "2000000"});
    m_baudCombo->setCurrentText("256000");
    m_baudCombo->setToolTip(tr("Select baud rate"));
    toolbar->addWidget(m_baudCombo);

    m_refreshBtn = new QPushButton(tr("Refresh"));
    m_refreshBtn->setToolTip(tr("Refresh port list"));
    toolbar->addWidget(m_refreshBtn);

    toolbar->addSeparator();

    m_connectBtn = new QPushButton(tr("Connect"));
    m_connectBtn->setStyleSheet("QPushButton { color: #4CAF50; font-weight: bold; }");
    m_connectBtn->setToolTip(tr("Connect device"));
    toolbar->addWidget(m_connectBtn);

    m_disconnectBtn = new QPushButton(tr("Disconnect"));
    m_disconnectBtn->setEnabled(false);
    m_disconnectBtn->setStyleSheet("QPushButton { color: #F44336; }");
    m_disconnectBtn->setToolTip(tr("Disconnect device"));
    toolbar->addWidget(m_disconnectBtn);

    toolbar->addSeparator();

    m_addWidgetBtn = new QPushButton(tr("+"));
    m_addWidgetBtn->setFixedWidth(36);
    m_addWidgetBtn->setToolTip(tr("Add dynamic widget (Button / Slider / Input)"));
    m_addWidgetBtn->setStyleSheet(
        "QPushButton { color: #1976D2; font-weight: bold; font-size: 16px; }"
        "QPushButton:hover { color: #2196F3; }"
    );
    toolbar->addWidget(m_addWidgetBtn);
}

void MainWindow::setupStatusBar()
{
    m_connectionStatus = new QLabel(tr(" Disconnected "));
    m_connectionStatus->setStyleSheet("QLabel { color: #9E9E9E; font-weight: bold; }");
    statusBar()->addWidget(m_connectionStatus);

    m_dataRate = new QLabel(tr(" Data: 0 fps "));
    statusBar()->addPermanentWidget(m_dataRate);

    statusBar()->showMessage(tr("Ready"), 3000);
}

// ============================================================
// Industrial IDE-style layout:
//   Left nav (QDockWidget) | Central QStackedWidget | Property dock (right) | Log dock (bottom)
// ============================================================

void MainWindow::setupCentralWidget()
{
    // Central workspace — QStackedWidget holds all tool pages
    m_workspaceStack = new QStackedWidget();
    setCentralWidget(m_workspaceStack);

    // Left navigation panel
    setupNavPanel();

    // Right property panel
    setupPropertyDock();

    // Right dynamic widget dock (WI-012)
    setupDynamicWidgetDock();

    // Bottom log console
    setupLogConsole();
}

void MainWindow::setupNavPanel()
{
    QDockWidget *navDock = new QDockWidget(tr("Navigation"), this);
    navDock->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);
    navDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);

    m_navList = new QListWidget();
    m_navList->setIconSize(QSize(24, 24));
    m_navList->setSpacing(4);
    m_navList->setMinimumWidth(160);
    m_navList->setMaximumWidth(220);

    // Navigation items (at least 5 entries as required)
    m_navList->addItem(tr("Dashboard"));
    m_navList->addItem(tr("Oscilloscope"));
    m_navList->addItem(tr("Automation"));
    m_navList->addItem(tr("Device"));
    m_navList->addItem(tr("Settings"));

    // Style the nav list with modern light theme
    m_navList->setStyleSheet(
        "QListWidget {"
        "  background-color: #FFFFFF;"
        "  color: #212121;"
        "  border: none;"
        "  border-right: 1px solid #E0E0E0;"
        "  font-size: 13px;"
        "  padding: 8px 0;"
        "}"
        "QListWidget::item {"
        "  padding: 10px 16px;"
        "  border-left: 3px solid transparent;"
        "}"
        "QListWidget::item:selected {"
        "  background-color: #E3F2FD;"
        "  color: #1976D2;"
        "  border-left: 3px solid #2196F3;"
        "  font-weight: bold;"
        "}"
        "QListWidget::item:hover {"
        "  background-color: #F5F5F5;"
        "}"
    );

    // Connect nav selection to workspace and property stack switching
    connect(m_navList, &QListWidget::currentRowChanged, this, [this](int row) {
        m_workspaceStack->setCurrentIndex(row);
        if (m_propertyStack) {
            m_propertyStack->setCurrentIndex(row);
        }
        refreshPropertyPanel();
    });

    navDock->setWidget(m_navList);
    addDockWidget(Qt::LeftDockWidgetArea, navDock);

    // Select first item by default
    m_navList->setCurrentRow(0);
}

void MainWindow::setupPropertyDock()
{
    m_propertyDock = new QDockWidget(tr("Properties"), this);
    m_propertyDock->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);
    m_propertyDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    m_propertyDock->setMinimumWidth(200);

    m_propertyStack = new QStackedWidget();
    m_propertyStack->setStyleSheet(
        "QStackedWidget { background-color: #F5F7FA; }"
    );

    // ---- Shared style helpers for modern light theme ----
    auto makeTitle = [](const QString &text) -> QLabel* {
        QLabel *lbl = new QLabel(text);
        lbl->setStyleSheet(
            "color: #1976D2; font-size: 13px; font-weight: bold;"
            "padding: 8px; border-bottom: 1px solid #E0E0E0;"
        );
        return lbl;
    };

    auto makeLabel = [](const QString &text = QString()) -> QLabel* {
        QLabel *lbl = new QLabel(text);
        lbl->setWordWrap(true);
        lbl->setStyleSheet("color: #424242; font-size: 12px; padding: 4px 8px;");
        return lbl;
    };

    auto makeDimLabel = [](const QString &text) -> QLabel* {
        QLabel *lbl = new QLabel(text);
        lbl->setWordWrap(true);
        lbl->setAlignment(Qt::AlignCenter);
        lbl->setStyleSheet("color: #9E9E9E; font-size: 12px; padding: 20px;");
        return lbl;
    };

    // ---- Page 0: Dashboard properties ----
    {
        QWidget *page = new QWidget();
        QVBoxLayout *lay = new QVBoxLayout(page);
        lay->setContentsMargins(0, 0, 0, 0);
        lay->setSpacing(2);
        lay->addWidget(makeTitle(tr("Dashboard")));
        m_propDashboardChannels = makeLabel(tr("Subscribed channels: 0"));
        lay->addWidget(m_propDashboardChannels);
        m_propDashboardRate = makeLabel(tr("Refresh rate: 10 Hz"));
        lay->addWidget(m_propDashboardRate);
        lay->addStretch();
        m_propertyStack->addWidget(page); // index 0
    }

    // ---- Page 1: Oscilloscope properties ----
    {
        QWidget *page = new QWidget();
        QVBoxLayout *lay = new QVBoxLayout(page);
        lay->setContentsMargins(0, 0, 0, 0);
        lay->setSpacing(2);
        lay->addWidget(makeTitle(tr("Oscilloscope")));
        m_propOscilloscopeCurves = makeLabel(tr("Curves: 0"));
        lay->addWidget(m_propOscilloscopeCurves);
        m_propOscilloscopeFPS = makeLabel(tr("Target FPS: 30"));
        lay->addWidget(m_propOscilloscopeFPS);
        lay->addStretch();
        m_propertyStack->addWidget(page); // index 1
    }

    // ---- Page 2: Automation (no properties yet) ----
    {
        QWidget *page = new QWidget();
        QVBoxLayout *lay = new QVBoxLayout(page);
        lay->setContentsMargins(0, 0, 0, 0);
        lay->addWidget(makeTitle(tr("Automation")));
        lay->addWidget(makeDimLabel(tr("No properties for Automation")));
        lay->addStretch();
        m_propertyStack->addWidget(page); // index 2
    }

    // ---- Page 3: Device (no properties yet) ----
    {
        QWidget *page = new QWidget();
        QVBoxLayout *lay = new QVBoxLayout(page);
        lay->setContentsMargins(0, 0, 0, 0);
        lay->addWidget(makeTitle(tr("Device")));
        lay->addWidget(makeDimLabel(tr("No properties for Device")));
        lay->addStretch();
        m_propertyStack->addWidget(page); // index 3
    }

    // ---- Page 4: Settings properties ----
    {
        QWidget *page = new QWidget();
        QVBoxLayout *lay = new QVBoxLayout(page);
        lay->setContentsMargins(0, 0, 0, 0);
        lay->setSpacing(2);
        lay->addWidget(makeTitle(tr("Settings")));
        m_propSettingsParams = makeLabel(tr("Parameters: 0"));
        lay->addWidget(m_propSettingsParams);
        lay->addStretch();
        m_propertyStack->addWidget(page); // index 4
    }

    m_propertyDock->setWidget(m_propertyStack);
    addDockWidget(Qt::RightDockWidgetArea, m_propertyDock);
}

void MainWindow::setupLogConsole()
{
    m_logDock = new QDockWidget(tr("Log Console"), this);
    m_logDock->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);
    m_logDock->setAllowedAreas(Qt::BottomDockWidgetArea | Qt::TopDockWidgetArea);

    QWidget *container = new QWidget();
    QVBoxLayout *lay = new QVBoxLayout(container);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(2);

    // Toolbar with Clear and Auto-scroll buttons
    QHBoxLayout *toolbar = new QHBoxLayout();
    toolbar->setContentsMargins(4, 2, 4, 2);

    m_logClearBtn = new QPushButton(tr("Clear"));
    m_logClearBtn->setFixedHeight(24);
    m_logClearBtn->setStyleSheet(
        "QPushButton { background: #FFFFFF; color: #424242; border: 1px solid #E0E0E0; "
        "border-radius: 3px; padding: 0 12px; }"
        "QPushButton:hover { background: #F5F5F5; border-color: #2196F3; }"
    );

    m_logAutoScrollBtn = new QPushButton(tr("Auto-scroll: ON"));
    m_logAutoScrollBtn->setCheckable(true);
    m_logAutoScrollBtn->setChecked(true);
    m_logAutoScrollBtn->setFixedHeight(24);
    m_logAutoScrollBtn->setStyleSheet(
        "QPushButton { background: #FFFFFF; color: #424242; border: 1px solid #E0E0E0; "
        "border-radius: 3px; padding: 0 12px; }"
        "QPushButton:hover { background: #F5F5F5; border-color: #2196F3; }"
        "QPushButton:checked { background: #2196F3; color: #FFFFFF; border: 1px solid #2196F3; }"
    );

    toolbar->addWidget(m_logClearBtn);
    toolbar->addWidget(m_logAutoScrollBtn);
    toolbar->addStretch();
    lay->addLayout(toolbar);

    // Log output area
    m_logOutput = new QPlainTextEdit();
    m_logOutput->setReadOnly(true);
    m_logOutput->setMaximumBlockCount(5000);
    m_logOutput->setStyleSheet(
        "QPlainTextEdit {"
        "  background-color: #FAFAFA;"
        "  color: #212121;"
        "  border: 1px solid #E0E0E0;"
        "  font-family: 'Consolas', 'Courier New', monospace;"
        "  font-size: 12px;"
        "}"
    );
    lay->addWidget(m_logOutput);

    m_logDock->setWidget(container);
    addDockWidget(Qt::BottomDockWidgetArea, m_logDock);

    // Connect Clear button
    connect(m_logClearBtn, &QPushButton::clicked, m_logOutput, &QPlainTextEdit::clear);

    // Connect Auto-scroll toggle
    connect(m_logAutoScrollBtn, &QPushButton::toggled, this, [this](bool checked) {
        m_logAutoScrollBtn->setText(checked ? tr("Auto-scroll: ON") : tr("Auto-scroll: OFF"));
    });
}

// ============================================================
// Static accessor and logging
// ============================================================

MainWindow* MainWindow::instance()
{
    return s_instance;
}

void MainWindow::log(const QString &message)
{
    if (!s_instance || !s_instance->m_logOutput) return;

    QString timestamp = QDateTime::currentDateTime().toString("hh:mm:ss.zzz");
    QString entry = QString("[%1] %2").arg(timestamp, message);

    s_instance->m_logOutput->appendPlainText(entry);

    // Auto-scroll to bottom if enabled
    if (s_instance->m_logAutoScrollBtn && s_instance->m_logAutoScrollBtn->isChecked()) {
        QTextCursor cursor = s_instance->m_logOutput->textCursor();
        cursor.movePosition(QTextCursor::End);
        s_instance->m_logOutput->setTextCursor(cursor);
    }
}

// Static instance pointer (defined in header with inline init via constructor)
// Note: s_instance is declared static in header; definition in cpp
MainWindow *MainWindow::s_instance = nullptr;

void MainWindow::setupConnections()
{
    connect(m_importProjectAction, &QAction::triggered, this, &MainWindow::onImportProject);
    connect(m_exportProjectAction, &QAction::triggered, this, &MainWindow::onExportProject);
    connect(m_exitAction, &QAction::triggered, qApp, &QApplication::quit);
    connect(m_settingsAction, &QAction::triggered, this, &MainWindow::onSettings);
    connect(m_channelConfigAction, &QAction::triggered, this, &MainWindow::onChannelConfig);
    connect(m_aboutAction, &QAction::triggered, this, &MainWindow::onAbout);

    connect(m_connectBtn, &QPushButton::clicked, this, &MainWindow::onConnect);
    connect(m_disconnectBtn, &QPushButton::clicked, this, &MainWindow::onDisconnect);
    connect(m_refreshBtn, &QPushButton::clicked, this, &MainWindow::refreshSerialPorts);
    connect(m_addWidgetBtn, &QPushButton::clicked, this, &MainWindow::onAddDynamicWidget);

    // WI-012: Route dynamic widget commands to the log console
    connect(m_widgetBindingManager, &MotorStudio::WidgetBindingManager::commandExecuted,
            this, [](const QString& cmd) {
        MainWindow::log(QString("[Command] %1").arg(cmd));
    });
}

// ============================================================
// P0-03: Communication Thread
// SerialTransport + VofaParser run on dedicated QThread.
// All cross-thread signals use Qt::QueuedConnection (automatic).
// ============================================================

void MainWindow::setupCommThread()
{
    m_commThread = new QThread(this);
    m_deviceWorker = new DeviceWorker();  // No parent — will be moved to thread

    // Move worker to comm thread
    m_deviceWorker->moveToThread(m_commThread);

    // Connect worker signals → MainWindow slots (QueuedConnection auto)
    connect(m_deviceWorker, &DeviceWorker::deviceConnected,
            this, &MainWindow::onDeviceConnected);
    connect(m_deviceWorker, &DeviceWorker::deviceDisconnected,
            this, &MainWindow::onDeviceDisconnected);
    connect(m_deviceWorker, &DeviceWorker::deviceError,
            this, &MainWindow::onDeviceError);
    connect(m_deviceWorker, &DeviceWorker::frameReady,
            this, &MainWindow::onFrameReady);

    // Clean up worker when thread finishes
    connect(m_commThread, &QThread::finished,
            m_deviceWorker, &QObject::deleteLater);

    m_commThread->start();
}

// ============================================================
// P0-02: DataBus-centric data pipeline
//
//   Comm Thread:              UI Thread:
//   SerialTransport           DataBus::publishFrame()
//     → VofaParser               ├→ CurveEngine::append()
//     → DeviceWorker              │   (ring buffer store)
//         → frameReady ──────→  onFrameReady()
//                                   └→ ChannelManager (CSV compat)
//   CurveWidget ← 30fps pull ← CurveEngine (LTTB downsample)
//   DashboardWidget ← 10fps poll ← DataBus::latestValue()
// ============================================================

void MainWindow::setupDataPipeline()
{
    // Pipeline is driven by onFrameReady() which receives data from comm thread.
    // Topic registration and DataBus publishing happen there.
}

// ============================================================
// P3-01: Automation Engine Setup
//
//   AutomationEngine runs on a dedicated worker thread (via TestRunner).
//   Callbacks thread-safely bridge to ParameterManager on the main thread.
//   AutomationWidget provides the UI and connects to engine signals.
// ============================================================

void MainWindow::setupAutomation()
{
    // Create engine without parent — it will be moved to TestRunner's worker thread
    m_automationEngine = new MotorStudio::AutomationEngine();

    // Register callbacks that bridge from worker thread back to main thread
    // SetParameter: safely call ParameterManager::setValueByName on main thread
    m_automationEngine->setSetParamCallback([this](const std::string& name, const std::string& value) -> bool {
        bool result = false;
        QMetaObject::invokeMethod(m_paramManager, "setValueByName", Qt::BlockingQueuedConnection,
                                  Q_RETURN_ARG(bool, result),
                                  Q_ARG(QString, QString::fromStdString(name)),
                                  Q_ARG(QVariant, QVariant(QString::fromStdString(value))));
        return result;
    });

    // ReadParameter: safely read from ParameterManager on main thread
    m_automationEngine->setReadParamCallback([this](const std::string& name) -> std::string {
        QVariant val;
        QMetaObject::invokeMethod(m_paramManager, "valueByName", Qt::BlockingQueuedConnection,
                                  Q_RETURN_ARG(QVariant, val),
                                  Q_ARG(QString, QString::fromStdString(name)));
        return val.toString().toStdString();
    });

    // Motor control callbacks (stub — real implementation requires communication layer)
    m_automationEngine->setMotorStartCallback([]() -> bool {
        MainWindow::log("Automation: MotorStart requested (stub)");
        return true;
    });

    m_automationEngine->setMotorStopCallback([]() -> bool {
        MainWindow::log("Automation: MotorStop requested (stub)");
        return true;
    });

    // Create TestRunner — this creates the worker thread and moves the engine to it
    m_testRunner = new MotorStudio::TestRunner(m_automationEngine, this);

    // Connect TestRunner::runnerFinished for programmatic use
    connect(m_testRunner, &MotorStudio::TestRunner::runnerFinished, this, [](const MotorStudio::TestResult& result) {
        MainWindow::log(QString("Automation test finished: %1 — %2")
            .arg(QString::fromStdString(result.caseName))
            .arg(result.passed ? "PASS" : "FAIL"));
    });

    // Create the UI widget — connects to engine signals for live updates
    m_automationWidget = new MotorStudio::AutomationWidget(m_automationEngine);

    MainWindow::log("Automation engine initialized");
}

void MainWindow::onFrameReady(const QVector<float>& values)
{
    auto& registry = MotorStudio::TopicRegistry::instance();
    auto& bus = MotorStudio::DataBus::instance();

    // Auto-register topics on first frame or when channel count changes
    if (m_topicIds.size() != static_cast<size_t>(values.size())) {
        // Unsubscribe old engine subscription
        if (m_engineSubscriberId != 0) {
            bus.unsubscribe(m_engineSubscriberId);
        }

        m_topicIds.clear();
        for (int i = 0; i < values.size(); ++i) {
            // Build a ChannelDescriptor with a default color from the palette
            MotorStudio::ChannelDescriptor desc;
            desc.name = QString("CH%1").arg(i + 1).toStdString();
            desc.unit = "";
            desc.dataType = "float";
            desc.scale = 1.0f;
            desc.offset = 0.0f;
            desc.defaultValue = 0.0f;

            // Cycle through the default color palette
            const QColor& col = MotorStudio::ChannelConfigDialog::kDefaultColors[
                i % MotorStudio::ChannelConfigDialog::kNumDefaultColors];
            QRgb rgba = col.rgba();
            desc.color = static_cast<uint32_t>(rgba);

            MotorStudio::TopicId tid = registry.registerTopic(desc);

            if (!m_curveEngine->hasChannel(tid)) {
                m_curveEngine->addChannel(tid);
            }

            m_topicIds.push_back(tid);
        }

        // Re-subscribe CurveEngine to all topics
        m_engineSubscriberId = bus.subscribeMultiple(m_topicIds, [this](const MotorStudio::DataPoint& dp) {
            m_curveEngine->append(dp);
        });

        // Refresh dashboard cells
        if (m_dashboardWidget) {
            m_dashboardWidget->subscribeToDataBus(10);
        }

        // Refresh property panel with new channel count
        refreshPropertyPanel();
    }

    uint64_t ts = QDateTime::currentMSecsSinceEpoch() * 1000ULL;

    // Publish to DataBus on UI thread → triggers CurveEngine subscriber
    bus.publishFrame(m_topicIds, values, ts);

    // ChannelManager for CSV backward compat
    m_channelManager->pushFrame(values, ts);

    // Data rate
    static int frameCount = 0;
    static QElapsedTimer timer;
    if (!timer.isValid()) timer.start();
    frameCount++;
    if (timer.elapsed() >= 1000) {
        double fps = frameCount * 1000.0 / timer.elapsed();
        m_dataRate->setText(QString(" Data: %1 fps ").arg(fps, 0, 'f', 0));
        frameCount = 0;
        timer.restart();
    }
}

void MainWindow::onDeviceConnected()
{
    m_connectionStatus->setText(tr(" Connected "));
    m_connectionStatus->setStyleSheet("QLabel { color: #4CAF50; font-weight: bold; }");
    m_connectBtn->setEnabled(false);
    m_disconnectBtn->setEnabled(true);
    m_portCombo->setEnabled(false);
    m_baudCombo->setEnabled(false);
    statusBar()->showMessage(tr("Connected"), 5000);
}

void MainWindow::onDeviceDisconnected()
{
    m_connectionStatus->setText(tr(" Disconnected "));
    m_connectionStatus->setStyleSheet("QLabel { color: #F44336; font-weight: bold; }");
    m_connectBtn->setEnabled(true);
    m_disconnectBtn->setEnabled(false);
    m_portCombo->setEnabled(true);
    m_baudCombo->setEnabled(true);
    statusBar()->showMessage(tr("Disconnected"), 3000);
}

void MainWindow::onDeviceError(const QString& message)
{
    statusBar()->showMessage(tr("Error: %1").arg(message), 10000);
}

// ============================================================
// Page management
// ============================================================

int MainWindow::addPage(const QString &title, QWidget *widget)
{
    // Still add to m_tabWidget if it exists (legacy path), but primary is QStackedWidget
    int index = m_workspaceStack ? m_workspaceStack->addWidget(widget) : -1;
    if (m_tabWidget) {
        m_tabWidget->addTab(widget, title);
    }
    // Update nav list item text if it exists at this index
    if (m_navList && index >= 0 && index < m_navList->count()) {
        // Nav items are pre-created; titles match by convention
    }
    if (index >= 0) {
        m_workspaceStack->setCurrentIndex(index);
    }
    return index;
}

void MainWindow::removePage(int index)
{
    if (!m_workspaceStack) return;
    QWidget *w = m_workspaceStack->widget(index);
    m_workspaceStack->removeWidget(w);
    if (m_tabWidget) {
        m_tabWidget->removeTab(index);
    }
    if (w) {
        w->deleteLater();
    }
}

// ============================================================
// Slots
// ============================================================

void MainWindow::onImportProject()
{
    QString path = QFileDialog::getOpenFileName(
        this, tr("Import Project File"), QString(),
        tr("JSON Files (*.json);;All Files (*)"));
    if (!path.isEmpty()) {
        emit projectImportRequested(path);
        statusBar()->showMessage(tr("Project imported: %1").arg(path), 5000);
    }
}

void MainWindow::onExportProject()
{
    QString path = QFileDialog::getSaveFileName(
        this, tr("Export Project File"), QString(),
        tr("JSON Files (*.json);;All Files (*)"));
    if (!path.isEmpty()) {
        emit projectExportRequested(path);
        statusBar()->showMessage(tr("Project exported: %1").arg(path), 5000);
    }
}

void MainWindow::onSettings()
{
    QMessageBox::information(this, tr("Preferences"), tr("Settings will be available in a future version."));
}

void MainWindow::onChannelConfig()
{
    MotorStudio::ChannelConfigDialog dlg(this);
    dlg.loadFromRegistry();
    if (dlg.exec() == QDialog::Accepted) {
        dlg.applyToRegistry();
        // Refresh dashboard with updated channel names
        if (m_dashboardWidget) {
            m_dashboardWidget->subscribeToDataBus(10);
        }
        statusBar()->showMessage(tr("Channel configuration updated"), 3000);
    }
}

void MainWindow::onAbout()
{
    QString aboutText = QString(
        "<h2>Motor Automation v0.1</h2>"
        "<p>Industrial Motor Debug & Automation Test Platform</p>"
        "<p>Tech: Qt5 / C++17 / qmake</p>"
        "<p>Target: Windows</p>"
        "<hr>"
        "<p>Internal Tool — Not for Commercial Use</p>");
    QMessageBox::about(this, tr("About Motor Automation"), aboutText);
}

void MainWindow::onConnect()
{
    QString port = m_portCombo->currentData().toString();
    if (port.isEmpty()) {
        port = m_portCombo->currentText().split('(').first().trimmed();
    }
    if (port.isEmpty()) {
        statusBar()->showMessage(tr("No port selected"), 3000);
        return;
    }

    int baud = m_baudCombo->currentText().toInt();

    QJsonObject config;
    config["port"] = port;
    config["baud"] = baud;
    QString configStr = QString::fromUtf8(QJsonDocument(config).toJson(QJsonDocument::Compact));

    // Invoke on comm thread via queued connection
    QMetaObject::invokeMethod(m_deviceWorker, "openDevice",
                              Qt::QueuedConnection,
                              Q_ARG(QString, configStr));
}

void MainWindow::onDisconnect()
{
    QMetaObject::invokeMethod(m_deviceWorker, "closeDevice",
                              Qt::QueuedConnection);
}

void MainWindow::refreshSerialPorts()
{
    QString current = m_portCombo->currentText();
    m_portCombo->blockSignals(true);
    m_portCombo->clear();

    const auto ports = QSerialPortInfo::availablePorts();
    for (const auto &port : ports) {
        QString desc = QString("%1 (%2)").arg(port.portName(), port.description());
        m_portCombo->addItem(desc, port.portName());
    }

    if (!current.isEmpty()) {
        int idx = m_portCombo->findText(current, Qt::MatchContains);
        if (idx >= 0) {
            m_portCombo->setCurrentIndex(idx);
        }
    }

    m_portCombo->blockSignals(false);
}

void MainWindow::createDefaultPages()
{
    // Pages added to QStackedWidget in order matching left navigation indices:
    //   0: Dashboard   -> DashboardWidget
    //   1: Oscilloscope -> CurveWidget
    //   2: Automation  -> placeholder (future)
    //   3: Device      -> FaultWidget
    //   4: Settings    -> ParameterWidget

    // Dashboard page (index 0)
    m_dashboardWidget = new MotorStudio::DashboardWidget();
    m_dashboardWidget->subscribeToDataBus(10);
    m_workspaceStack->addWidget(m_dashboardWidget);

    // Oscilloscope / Curve page (index 1) — WI-008 Multi-Window + WI-009 Manager Panel
    {
        m_curveContainer = new MotorStudio::MultiCurveContainer(m_curveEngine);
        m_curveContainer->attachCurveEngine(m_curveEngine, 30);

        m_curveManagerPanel = new MotorStudio::CurveManagerPanel();
        m_curveManagerPanel->setCurveContainer(m_curveContainer);
        m_curveManagerPanel->setCurveEngine(m_curveEngine);

        auto* oscSplitter = new QSplitter(Qt::Vertical);
        oscSplitter->addWidget(m_curveContainer);
        oscSplitter->addWidget(m_curveManagerPanel);
        oscSplitter->setStretchFactor(0, 3);  // Curves get 75%
        oscSplitter->setStretchFactor(1, 1);  // Manager gets 25%
        oscSplitter->setStyleSheet("QSplitter::handle { background: #E0E0E0; }");

        m_workspaceStack->addWidget(oscSplitter);
    }

    // Automation page (index 2)
    if (m_automationWidget) {
        m_workspaceStack->addWidget(m_automationWidget);
    } else {
        // Fallback placeholder if setupAutomation failed
        QWidget *placeholder = new QWidget();
        QVBoxLayout *lay = new QVBoxLayout(placeholder);
        QLabel *label = new QLabel(tr("Automation\n\nInitialization failed."));
        label->setAlignment(Qt::AlignCenter);
        label->setStyleSheet("color: #9E9E9E; font-size: 14px;");
        lay->addWidget(label);
        m_workspaceStack->addWidget(placeholder);
    }

    // Device / Fault page (index 3)
    m_faultWidget = new MotorStudio::FaultWidget();
    m_workspaceStack->addWidget(m_faultWidget);

    // Settings / Parameter page (index 4)
    m_paramWidget = new MotorStudio::ParameterWidget();
    m_paramWidget->setParameterManager(m_paramManager);
    m_workspaceStack->addWidget(m_paramWidget);

    // Select first page
    if (m_navList) {
        m_navList->setCurrentRow(0);
    }

    MainWindow::log(tr("Application started — Industrial layout initialized"));
}

// ============================================================
// Property panel refresh — updates dynamic property values
// for the currently visible page in the property dock.
// ============================================================

void MainWindow::refreshPropertyPanel()
{
    if (!m_propertyStack) return;
    int idx = m_propertyStack->currentIndex();

    switch (idx) {
    case 0: // Dashboard
        if (m_propDashboardChannels)
            m_propDashboardChannels->setText(tr("Subscribed channels: %1").arg(m_topicIds.size()));
        break;
    case 1: // Oscilloscope
        if (m_propOscilloscopeCurves)
            m_propOscilloscopeCurves->setText(tr("Curve widgets: %1").arg(
                m_curveContainer ? m_curveContainer->curveWidgetCount() : 0));
        break;
    case 4: // Settings
        if (m_propSettingsParams && m_paramManager)
            m_propSettingsParams->setText(tr("Parameters: %1").arg(m_paramManager->parameterCount()));
        break;
    default:
        break;
    }
}

// ============================================================
// CSV Export (reads from CurveEngine)
// ============================================================

void MainWindow::onExportCSV()
{
    QString path = QFileDialog::getSaveFileName(this, tr("Export CSV"), QString(), tr("CSV (*.csv)"));
    if (path.isEmpty()) return;

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        statusBar()->showMessage(tr("Cannot write file"), 5000);
        return;
    }

    QTextStream stream(&file);

    auto& registry = MotorStudio::TopicRegistry::instance();

    stream << "Timestamp";
    for (auto tid : m_topicIds) {
        QString name = QString::fromStdString(registry.topicName(tid));
        stream << "," << (name.isEmpty() ? QString("CH%1").arg(tid) : name);
    }
    stream << "\n";

    if (!m_topicIds.empty()) {
        auto* firstCh = m_curveEngine->channel(m_topicIds[0]);
        if (firstCh) {
            auto allPoints = firstCh->allPoints();
            for (const auto& p : allPoints) {
                stream << p.first;
                for (auto tid : m_topicIds) {
                    auto* ch = m_curveEngine->channel(tid);
                    if (ch) {
                        auto chPoints = ch->allPoints();
                        float val = 0;
                        for (const auto& cp : chPoints) {
                            if (cp.first >= p.first) {
                                val = cp.second;
                                break;
                            }
                            val = cp.second;
                        }
                        stream << "," << val;
                    } else {
                        stream << ",";
                    }
                }
                stream << "\n";
            }
        }
    }

    file.close();
    statusBar()->showMessage(tr("CSV exported: %1").arg(path), 5000);
}

// ============================================================
// Project file save/load
// ============================================================

void MainWindow::onSaveProject()
{
    QString path = QFileDialog::getSaveFileName(this, tr("Save Project"), QString(), tr("JSON (*.json)"));
    if (path.isEmpty()) return;

    QJsonObject project;
    project["name"] = "Motor Automation Project";
    project["version"] = "0.1";
    project["serialPort"] = m_portCombo->currentText();
    project["baudRate"] = m_baudCombo->currentText().toInt();
    project["parameters"] = m_paramManager->toJson();
    project["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);

    QFile file(path);
    if (file.open(QIODevice::WriteOnly)) {
        QJsonDocument doc(project);
        file.write(doc.toJson(QJsonDocument::Indented));
        file.close();
        statusBar()->showMessage(tr("Project saved: %1").arg(path), 5000);
    }
}

void MainWindow::onLoadProject()
{
    QString path = QFileDialog::getOpenFileName(this, tr("Open Project"), QString(), tr("JSON (*.json)"));
    if (path.isEmpty()) return;

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        statusBar()->showMessage(tr("Cannot open file"), 5000);
        return;
    }

    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();

    if (!doc.isObject()) return;
    QJsonObject project = doc.object();

    if (project.contains("parameters")) {
        m_paramManager->fromJson(project["parameters"].toObject());
        m_paramWidget->refresh();
    }

    statusBar()->showMessage(tr("Project loaded: %1").arg(path), 5000);
}

// ============================================================
// WI-012: Dynamic Widget System
// ============================================================

void MainWindow::setupDynamicWidgetDock()
{
    m_dynamicWidgetDock = new QDockWidget(tr("Dynamic Widgets"), this);
    m_dynamicWidgetDock->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);
    m_dynamicWidgetDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    m_dynamicWidgetDock->setMinimumWidth(200);

    m_dynamicWidgetContainer = new QWidget();
    m_dynamicWidgetContainer->setStyleSheet("background-color: #FFFFFF;");
    m_dynamicWidgetLayout = new QVBoxLayout(m_dynamicWidgetContainer);
    m_dynamicWidgetLayout->setContentsMargins(8, 8, 8, 8);
    m_dynamicWidgetLayout->setSpacing(6);

    // Title label
    QLabel *title = new QLabel(tr("Interactive Controls"));
    title->setStyleSheet(
        "color: #1976D2; font-size: 13px; font-weight: bold;"
        "padding: 8px; border-bottom: 1px solid #E0E0E0;"
    );
    m_dynamicWidgetLayout->addWidget(title);

    // Hint when no widgets exist
    QLabel *hint = new QLabel(tr("Click [+] in toolbar\nto add a widget"));
    hint->setAlignment(Qt::AlignCenter);
    hint->setWordWrap(true);
    hint->setStyleSheet("color: #9E9E9E; font-size: 11px; padding: 12px;");
    hint->setObjectName("dynamicWidgetHint");
    m_dynamicWidgetLayout->addWidget(hint);

    // Spacer at bottom
    m_dynamicWidgetLayout->addStretch();

    m_dynamicWidgetDock->setWidget(m_dynamicWidgetContainer);
    addDockWidget(Qt::RightDockWidgetArea, m_dynamicWidgetDock);
}

void MainWindow::onAddDynamicWidget()
{
    // Remove hint label if present
    QLabel *hint = m_dynamicWidgetContainer->findChild<QLabel*>("dynamicWidgetHint");
    if (hint) {
        hint->hide();
        hint->deleteLater();
    }

    // ---- Creation dialog ----
    QDialog dlg(this);
    dlg.setWindowTitle(tr("Add Dynamic Widget"));
    dlg.setMinimumWidth(360);
    dlg.setStyleSheet(
        "QDialog { background-color: #FFFFFF; }"
        "QLabel { color: #212121; font-size: 12px; }"
    );

    QVBoxLayout *dlgLayout = new QVBoxLayout(&dlg);
    dlgLayout->setSpacing(8);

    // Type
    dlgLayout->addWidget(new QLabel(tr("Widget Type:")));
    QComboBox *typeCombo = new QComboBox();
    typeCombo->addItems({tr("Button"), tr("Slider"), tr("Input")});
    typeCombo->setStyleSheet(
        "QComboBox { background: #FFFFFF; color: #424242; border: 1px solid #E0E0E0;"
        "  border-radius: 3px; padding: 4px 8px; }"
        "QComboBox::drop-down { border: none; }"
        "QComboBox QAbstractItemView { background: #FFFFFF; color: #424242;"
        "  selection-background-color: #2196F3; border: 1px solid #E0E0E0; }"
    );
    dlgLayout->addWidget(typeCombo);

    // Label
    dlgLayout->addWidget(new QLabel(tr("Label:")));
    QLineEdit *labelEdit = new QLineEdit();
    labelEdit->setPlaceholderText(tr("e.g., Start Motor"));
    labelEdit->setStyleSheet(
        "QLineEdit { background: #FAFAFA; color: #424242; border: 1px solid #E0E0E0;"
        "  border-radius: 3px; padding: 4px 8px; }"
        "QLineEdit:focus { border-color: #1976D2; }"
    );
    dlgLayout->addWidget(labelEdit);

    // Command
    dlgLayout->addWidget(new QLabel(tr("Binding Command ({value} = widget value):")));
    QLineEdit *cmdEdit = new QLineEdit();
    cmdEdit->setPlaceholderText(tr("e.g., Set Speed {value}"));
    cmdEdit->setStyleSheet(labelEdit->styleSheet());
    dlgLayout->addWidget(cmdEdit);

    // Buttons
    QDialogButtonBox *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    buttons->setStyleSheet(
        "QPushButton { background: #FFFFFF; color: #424242; border: 1px solid #E0E0E0;"
        "  border-radius: 3px; padding: 6px 20px; min-width: 70px; }"
        "QPushButton:hover { background: #F5F5F5; }"
        "QPushButton:default { background: #2196F3; color: #fff; border-color: #2196F3; }"
    );
    connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    dlgLayout->addWidget(buttons);

    if (dlg.exec() != QDialog::Accepted) return;

    QString label = labelEdit->text().trimmed();
    if (label.isEmpty()) {
        QMessageBox::warning(this, tr("Missing Label"), tr("Please enter a label for the widget."));
        return;
    }

    // Map combo selection to factory type string
    QString type;
    switch (typeCombo->currentIndex()) {
    case 0: type = "button"; break;
    case 1: type = "slider"; break;
    case 2: type = "input"; break;
    }

    QWidget *widget = MotorStudio::DynamicWidgetFactory::createWidget(type);
    if (!widget) {
        MainWindow::log("Error: Failed to create dynamic widget of type: " + type);
        return;
    }

    // Set object name and label
    widget->setObjectName(label);
    if (auto *btn = qobject_cast<QPushButton*>(widget)) {
        btn->setText(label);
    }

    // Right-click context menu
    widget->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(widget, &QWidget::customContextMenuRequested,
            this, &MainWindow::onDynamicWidgetContextMenu);

    // Build row: label + widget
    QWidget *row = new QWidget();
    row->setStyleSheet("background: transparent;");
    QHBoxLayout *rowLayout = new QHBoxLayout(row);
    rowLayout->setContentsMargins(0, 2, 0, 2);
    rowLayout->setSpacing(8);

    QLabel *nameLabel = new QLabel(label + ":");
    nameLabel->setStyleSheet("color: #424242; font-size: 12px;");
    nameLabel->setMinimumWidth(50);
    nameLabel->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Preferred);
    rowLayout->addWidget(nameLabel);
    rowLayout->addWidget(widget, 1);

    // Insert before the stretch spacer (last item in layout)
    int lastIdx = m_dynamicWidgetLayout->count() - 1;
    m_dynamicWidgetLayout->insertWidget(lastIdx, row);

    // Bind command
    QString cmd = cmdEdit->text().trimmed();
    if (!cmd.isEmpty()) {
        m_widgetBindingManager->bind(widget, cmd);
    }

    MainWindow::log(QString("Dynamic widget created: %1 [%2]%3")
        .arg(label, type, cmd.isEmpty() ? QString() : QString(" -> %1").arg(cmd)));
}

void MainWindow::onDynamicWidgetContextMenu(const QPoint &pos)
{
    QWidget *widget = qobject_cast<QWidget*>(sender());
    if (!widget) return;

    QMenu menu;
    menu.setStyleSheet(
        "QMenu { background-color: #FFFFFF; color: #424242; border: 1px solid #E0E0E0; padding: 4px; }"
        "QMenu::item { padding: 6px 24px; }"
        "QMenu::item:selected { background-color: #2196F3; }"
    );

    QAction *editAction = menu.addAction(tr("Edit Binding"));
    QAction *deleteAction = menu.addAction(tr("Delete"));

    QAction *chosen = menu.exec(widget->mapToGlobal(pos));
    if (!chosen) return;

    if (chosen == editAction) {
        // Show edit dialog
        bool ok = false;
        QString current = m_widgetBindingManager->command(widget);
        QString newCmd = QInputDialog::getText(this, tr("Edit Binding"),
            tr("Command (use {value} for widget value):"),
            QLineEdit::Normal, current, &ok);
        if (ok && !newCmd.trimmed().isEmpty()) {
            m_widgetBindingManager->bind(widget, newCmd.trimmed());
            MainWindow::log(QString("Binding updated for: %1 -> %2")
                .arg(widget->objectName(), newCmd.trimmed()));
        }
    } else if (chosen == deleteAction) {
        // Remove the row widget (the direct parent of the widget in the dock)
        QWidget *row = widget->parentWidget();
        m_widgetBindingManager->unbind(widget);

        if (row && row != m_dynamicWidgetContainer) {
            m_dynamicWidgetLayout->removeWidget(row);
            row->hide();
            row->deleteLater();
        }

        MainWindow::log(QString("Dynamic widget deleted: %1").arg(widget->objectName()));

        // Show hint again if no row widgets remain
        bool hasRows = false;
        for (int i = 0; i < m_dynamicWidgetLayout->count(); ++i) {
            QLayoutItem *item = m_dynamicWidgetLayout->itemAt(i);
            if (item->spacerItem()) continue;
            QWidget *w = item->widget();
            if (!w) continue;
            // Title and hint are QLabel; rows are plain QWidget containers
            if (qobject_cast<QLabel*>(w)) continue;
            hasRows = true;
            break;
        }

        if (!hasRows) {
            QLabel *hint = new QLabel(tr("Click [+] in toolbar\nto add a widget"));
            hint->setAlignment(Qt::AlignCenter);
            hint->setWordWrap(true);
            hint->setStyleSheet("color: #9E9E9E; font-size: 11px; padding: 12px;");
            hint->setObjectName("dynamicWidgetHint");
            int lastIdx = m_dynamicWidgetLayout->count() - 1;
            m_dynamicWidgetLayout->insertWidget(lastIdx, hint);
        }
    }
}
