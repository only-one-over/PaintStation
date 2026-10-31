#include "MainWindow.h"
#include "ConfigPage.h"
#include "CalibVerifyPage.h"
#include "PointManagerPage.h"
#include "FlowEditPage.h"
#include "AppConfig.h"
#include "RobotController.h"
#include "RealSenseCamera.h"
#include "HandEyeCalibrator.h"
#include "PointStore.h"

#include <QTabWidget>
#include <QListWidget>
#include <QStackedWidget>
#include <QStatusBar>
#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QApplication>
#include <QDateTime>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QFrame>
#include <QLabel>
#include <opencv2/opencv.hpp>

static QFile g_logFile;
static void messageHandler(QtMsgType, const QMessageLogContext&, const QString& msg)
{
    if (!g_logFile.isOpen()) return;
    QTextStream ts(&g_logFile);
    ts << QDateTime::currentDateTime().toString("HH:mm:ss.zzz")
       << "  " << msg << "\n";
}

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent)
{
    QFile qss(QDir::current().filePath("resources/style.qss"));
    if (qss.open(QIODevice::ReadOnly)) {
        qApp->setStyleSheet(QTextStream(&qss).readAll());
        qss.close();
    }

    m_cfg    = std::make_unique<AppConfig>();
    m_robot  = std::make_unique<RobotController>();
    m_cam    = std::make_unique<RealSenseCamera>();
    m_calib  = std::make_unique<HandEyeCalibrator>();
    m_points = std::make_unique<PointStore>();

    m_cfg->ensureDirs();

    QString logPath = QDir::current().filePath(
        m_cfg->paths.logDir + "/" + QDateTime::currentDateTime().toString("yyyy-MM-dd") + ".log");
    g_logFile.setFileName(logPath);
    g_logFile.open(QIODevice::WriteOnly | QIODevice::Append);
    qInstallMessageHandler(messageHandler);

    m_cfg->load(QDir::current().filePath(m_cfg->paths.appConfigFile));

    // 加载已有点位
    m_points->load(QDir::current().filePath("userdata/points/points.json"));

    buildUi();

    // 状态条 500ms 刷新
    connect(&m_statusTimer, &QTimer::timeout, this, &MainWindow::refreshStatusBar);
    m_statusTimer.start(500);
    refreshStatusBar();
}

MainWindow::~MainWindow()
{
    delete takeCentralWidget();
    if (m_cam) m_cam->close();
}

void MainWindow::buildUi()
{
    setWindowTitle("PaintStation - 喷涂工作站 v0.2");
    resize(1400, 850);

    auto* central = new QWidget;
    auto* root = new QVBoxLayout(central);
    root->setContentsMargins(0,0,0,0); root->setSpacing(0);

    // ---- 顶部状态条 ----
    auto* topBar = new QFrame;
    topBar->setObjectName("topBar");
    topBar->setFixedHeight(42);
    auto* topLay = new QHBoxLayout(topBar);
    topLay->setContentsMargins(16,6,16,6); topLay->setSpacing(24);

    topLay->addWidget(new QLabel("PaintStation"));
    topLay->addSpacing(20);
    m_lblRobotState = new QLabel("机器人: ● 未连接");
    m_lblCamState   = new QLabel("相机: ● 未打开");
    m_lblPose       = new QLabel("X:--  Y:--  Z:--");
    topLay->addWidget(m_lblRobotState);
    topLay->addWidget(m_lblCamState);
    topLay->addWidget(m_lblPose, 1);
    root->addWidget(topBar);

    // ---- 主体：左侧导航 + 右侧栈 ----
    auto* body = new QHBoxLayout;
    body->setContentsMargins(0,0,0,0); body->setSpacing(0);

    m_nav = new QListWidget;
    m_nav->setObjectName("navList");
    m_nav->setFixedWidth(150);
    m_nav->addItem("⚙  配置");
    m_nav->addItem("🎯  标定采集");
    m_nav->addItem("📍  点位管理");
    m_nav->addItem("▶  流程编辑");

    m_stack = new QStackedWidget;
    auto* p1 = new ConfigPage(m_cfg.get(), m_robot.get(), m_cam.get(), this);
    auto* p2 = new CalibVerifyPage(m_cfg.get(), m_robot.get(), m_cam.get(), m_calib.get(), this);
    auto* p3 = new PointManagerPage(m_robot.get(), m_points.get(), this);
    auto* p4 = new FlowEditPage(m_robot.get(), m_points.get(), this);
    connect(p3, &PointManagerPage::pointsChanged, p4, &FlowEditPage::refreshPointCombo);
    m_stack->addWidget(p1);
    m_stack->addWidget(p2);
    m_stack->addWidget(p3);
    m_stack->addWidget(p4);
    m_nav->setCurrentRow(0);

    body->addWidget(m_nav);
    body->addWidget(m_stack, 1);
    root->addLayout(body, 1);

    connect(m_nav, &QListWidget::currentRowChanged, m_stack, &QStackedWidget::setCurrentIndex);

    setCentralWidget(central);

    statusBar()->showMessage("就绪");
    connect(p1, &ConfigPage::statusMessage, this, [this](const QString& m){ statusBar()->showMessage(m); });
    connect(p2, &CalibVerifyPage::statusMessage, this, [this](const QString& m){ statusBar()->showMessage(m); });
    connect(p3, &PointManagerPage::statusMessage, this, [this](const QString& m){ statusBar()->showMessage(m); });
    connect(p4, &FlowEditPage::statusMessage, this, [this](const QString& m){ statusBar()->showMessage(m); });
}

void MainWindow::refreshStatusBar()
{
    bool rob = m_robot->isConnected();
    m_lblRobotState->setText(rob ? "机器人: ● 已连接" : "机器人: ● 未连接");
    m_lblRobotState->setStyleSheet(rob ? "color:#22c55e;font-weight:600;" : "color:#ef4444;font-weight:600;");

    bool cam = m_cam->isOpened();
    m_lblCamState->setText(cam ? "相机: ● 已打开" : "相机: ● 未打开");
    m_lblCamState->setStyleSheet(cam ? "color:#22c55e;font-weight:600;" : "color:#ef4444;font-weight:600;");

    if (rob) {
        ToolPose p;
        if (m_robot->getCurrentToolPose(p)) {
            m_lblPose->setText(QString("X:%1  Y:%2  Z:%3  |  A:%4  B:%5  C:%6")
                .arg(p.x,0,'f',1).arg(p.y,0,'f',1).arg(p.z,0,'f',1)
                .arg(p.a,0,'f',1).arg(p.b,0,'f',1).arg(p.c,0,'f',1));
        }
    } else {
        m_lblPose->setText("X:--  Y:--  Z:--");
    }
}
