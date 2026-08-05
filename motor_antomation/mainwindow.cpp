#include "mainwindow.h"

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
#include <QFileDialog>
#include <QMessageBox>
#include <QSerialPortInfo>
#include <QTimer>
#include <QSettings>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_tabWidget(nullptr)
{
    setWindowTitle("Motor Automation");
    resize(1200, 800);
    setMinimumSize(800, 600);

    setupMenuBar();
    setupToolBar();
    setupStatusBar();
    setupCentralWidget();
    setupConnections();

    // 初始刷新串口列表
    refreshSerialPorts();

    // 定时刷新串口列表
    m_portRefreshTimer = new QTimer(this);
    m_portRefreshTimer->setInterval(2000);
    connect(m_portRefreshTimer, &QTimer::timeout, this, &MainWindow::refreshSerialPorts);
    m_portRefreshTimer->start();

    // 恢复上次窗口状态
    QSettings settings;
    restoreGeometry(settings.value("MainWindow/geometry").toByteArray());
    restoreState(settings.value("MainWindow/state").toByteArray());
}

MainWindow::~MainWindow()
{
    QSettings settings;
    settings.setValue("MainWindow/geometry", saveGeometry());
    settings.setValue("MainWindow/state", saveState());
}

void MainWindow::setupMenuBar()
{
    // === 文件菜单 ===
    m_fileMenu = menuBar()->addMenu(tr("文件(&F)"));

    m_importProjectAction = new QAction(tr("导入工程..."), this);
    m_importProjectAction->setShortcut(QKeySequence("Ctrl+O"));
    m_fileMenu->addAction(m_importProjectAction);

    m_exportProjectAction = new QAction(tr("导出工程..."), this);
    m_exportProjectAction->setShortcut(QKeySequence("Ctrl+S"));
    m_fileMenu->addAction(m_exportProjectAction);

    m_fileMenu->addSeparator();

    m_exitAction = new QAction(tr("退出(&X)"), this);
    m_exitAction->setShortcut(QKeySequence("Alt+F4"));
    m_fileMenu->addAction(m_exitAction);

    // === 视图菜单 ===
    m_viewMenu = menuBar()->addMenu(tr("视图(&V)"));
    m_viewMenu->addAction(tr("默认布局"))->setEnabled(false);

    // === 设置菜单 ===
    m_settingsMenu = menuBar()->addMenu(tr("设置(&S)"));
    m_settingsAction = new QAction(tr("偏好设置..."), this);
    m_settingsMenu->addAction(m_settingsAction);

    // === 帮助菜单 ===
    m_helpMenu = menuBar()->addMenu(tr("帮助(&H)"));
    m_aboutAction = new QAction(tr("关于..."), this);
    m_helpMenu->addAction(m_aboutAction);
}

void MainWindow::setupToolBar()
{
    QToolBar *toolbar = addToolBar(tr("主工具栏"));
    toolbar->setMovable(false);
    toolbar->setIconSize(QSize(24, 24));

    // 串口选择
    toolbar->addWidget(new QLabel(tr("  串口: ")));
    m_portCombo = new QComboBox();
    m_portCombo->setMinimumWidth(120);
    m_portCombo->setToolTip(tr("选择串口"));
    toolbar->addWidget(m_portCombo);

    toolbar->addWidget(new QLabel(tr(" 波特率: ")));
    m_baudCombo = new QComboBox();
    m_baudCombo->setMinimumWidth(100);
    m_baudCombo->addItems({"9600", "19200", "38400", "57600", "115200", "256000", "460800", "921600", "1000000", "2000000"});
    m_baudCombo->setCurrentText("256000");
    m_baudCombo->setToolTip(tr("选择波特率"));
    toolbar->addWidget(m_baudCombo);

    m_refreshBtn = new QPushButton(tr("刷新"));
    m_refreshBtn->setToolTip(tr("刷新串口列表"));
    toolbar->addWidget(m_refreshBtn);

    toolbar->addSeparator();

    m_connectBtn = new QPushButton(tr("连接"));
    m_connectBtn->setStyleSheet("QPushButton { color: green; font-weight: bold; }");
    m_connectBtn->setToolTip(tr("连接设备"));
    toolbar->addWidget(m_connectBtn);

    m_disconnectBtn = new QPushButton(tr("断开"));
    m_disconnectBtn->setEnabled(false);
    m_disconnectBtn->setStyleSheet("QPushButton { color: red; }");
    m_disconnectBtn->setToolTip(tr("断开设备"));
    toolbar->addWidget(m_disconnectBtn);
}

void MainWindow::setupStatusBar()
{
    m_connectionStatus = new QLabel(tr(" 未连接 "));
    m_connectionStatus->setStyleSheet("QLabel { color: gray; font-weight: bold; }");
    statusBar()->addWidget(m_connectionStatus);

    m_dataRate = new QLabel(tr(" 数据: 0 B/s "));
    statusBar()->addPermanentWidget(m_dataRate);

    statusBar()->showMessage(tr("就绪"), 3000);
}

void MainWindow::setupCentralWidget()
{
    m_tabWidget = new QTabWidget();
    m_tabWidget->setTabsClosable(true);
    m_tabWidget->setMovable(true);
    m_tabWidget->setDocumentMode(true);

    // 连接信号：点击关闭标签页时移除
    connect(m_tabWidget, &QTabWidget::tabCloseRequested, this, &MainWindow::removePage);

    setCentralWidget(m_tabWidget);
}

void MainWindow::setupConnections()
{
    connect(m_importProjectAction, &QAction::triggered, this, &MainWindow::onImportProject);
    connect(m_exportProjectAction, &QAction::triggered, this, &MainWindow::onExportProject);
    connect(m_exitAction, &QAction::triggered, qApp, &QApplication::quit);
    connect(m_settingsAction, &QAction::triggered, this, &MainWindow::onSettings);
    connect(m_aboutAction, &QAction::triggered, this, &MainWindow::onAbout);

    connect(m_connectBtn, &QPushButton::clicked, this, &MainWindow::onConnect);
    connect(m_disconnectBtn, &QPushButton::clicked, this, &MainWindow::onDisconnect);
    connect(m_refreshBtn, &QPushButton::clicked, this, &MainWindow::refreshSerialPorts);
}

// === 页面管理 ===

int MainWindow::addPage(const QString &title, QWidget *widget)
{
    int index = m_tabWidget->addTab(widget, title);
    m_tabWidget->setCurrentIndex(index);
    return index;
}

void MainWindow::removePage(int index)
{
    QWidget *w = m_tabWidget->widget(index);
    m_tabWidget->removeTab(index);
    if (w) {
        w->deleteLater();
    }
}

// === 槽函数 ===

void MainWindow::onImportProject()
{
    QString path = QFileDialog::getOpenFileName(
        this, tr("导入工程文件"), QString(),
        tr("JSON 文件 (*.json);;所有文件 (*)"));
    if (!path.isEmpty()) {
        emit projectImportRequested(path);
        statusBar()->showMessage(tr("已导入工程: %1").arg(path), 5000);
    }
}

void MainWindow::onExportProject()
{
    QString path = QFileDialog::getSaveFileName(
        this, tr("导出工程文件"), QString(),
        tr("JSON 文件 (*.json);;所有文件 (*)"));
    if (!path.isEmpty()) {
        emit projectExportRequested(path);
        statusBar()->showMessage(tr("已导出工程: %1").arg(path), 5000);
    }
}

void MainWindow::onSettings()
{
    QMessageBox::information(this, tr("偏好设置"), tr("设置功能将在后续版本中实现。"));
}

void MainWindow::onAbout()
{
    QString aboutText = QString(
        "<h2>Motor Automation v0.1</h2>"
        "<p>工业级电机调试与自动化测试平台</p>"
        "<p>技术栈: Qt5 / C++17 / CMake</p>"
        "<p>目标平台: Windows</p>"
        "<hr>"
        "<p>内部工具 — 不商业化</p>");
    QMessageBox::about(this, tr("关于 Motor Automation"), aboutText);
}

void MainWindow::onConnect()
{
    QString port = m_portCombo->currentText();
    if (port.isEmpty()) {
        statusBar()->showMessage(tr("未选择串口"), 3000);
        return;
    }

    int baud = m_baudCombo->currentText().toInt();
    emit connectRequested(port, baud);

    m_connectionStatus->setText(tr(" 已连接: %1 @ %2 ").arg(port).arg(baud));
    m_connectionStatus->setStyleSheet("QLabel { color: green; font-weight: bold; }");

    m_connectBtn->setEnabled(false);
    m_disconnectBtn->setEnabled(true);
    m_portCombo->setEnabled(false);
    m_baudCombo->setEnabled(false);

    statusBar()->showMessage(tr("已连接 %1 @ %2 bps").arg(port).arg(baud), 5000);
}

void MainWindow::onDisconnect()
{
    emit disconnectRequested();

    m_connectionStatus->setText(tr(" 未连接 "));
    m_connectionStatus->setStyleSheet("QLabel { color: gray; font-weight: bold; }");

    m_connectBtn->setEnabled(true);
    m_disconnectBtn->setEnabled(false);
    m_portCombo->setEnabled(true);
    m_baudCombo->setEnabled(true);

    statusBar()->showMessage(tr("已断开"), 3000);
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

    // 恢复上次选择
    if (!current.isEmpty()) {
        int idx = m_portCombo->findText(current, Qt::MatchContains);
        if (idx >= 0) {
            m_portCombo->setCurrentIndex(idx);
        }
    }

    m_portCombo->blockSignals(false);
}