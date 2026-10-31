#include "ConfigPage.h"
#include "AppConfig.h"
#include "RobotController.h"
#include "Camera.h"
#include "HandEyeCalibrator.h"

#include <QFormLayout>
#include <QLineEdit>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QPushButton>
#include <QLabel>
#include <QGroupBox>
#include <QMessageBox>
#include <QFileInfo>
#include <QFileDialog>
#include <QDir>
#include <QStyle>

ConfigPage::ConfigPage(AppConfig* cfg, RobotController* robot, Camera* cam, HandEyeCalibrator* calib, QWidget* parent)
    : QWidget(parent), m_cfg(cfg), m_robot(robot), m_cam(cam), m_calib(calib)
{
    buildUi();
    loadFromCfg();

    connect(m_robot, &RobotController::connectionChanged, this, [this](bool ok, const QString& msg){
        m_lblRobotState->setText(ok ? "● 已连接" : "● 未连接");
        m_lblRobotState->setObjectName(ok ? "statusOk" : "statusBad");
        m_lblRobotState->style()->unpolish(m_lblRobotState);
        m_lblRobotState->style()->polish(m_lblRobotState);
        m_btnConnectRobot->setText(ok ? "断开" : "连接机器人");
        emit statusMessage(msg);
    });
}

void ConfigPage::buildUi()
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(20, 20, 20, 20);
    root->setSpacing(16);

    auto* title = new QLabel("系统配置");
    title->setObjectName("titleLabel");
    root->addWidget(title);

    auto* grid = new QGridLayout;
    grid->setSpacing(16);

    // 机器人
    auto* gbRob = new QGroupBox("机器人 (埃夫特 ER)");
    auto* fRob = new QFormLayout(gbRob);
    fRob->setSpacing(10); fRob->setContentsMargins(16, 20, 16, 16);
    m_ip    = new QLineEdit;
    m_port  = new QSpinBox; m_port->setRange(1,65535);
    m_speed = new QSpinBox; m_speed->setRange(1,100); m_speed->setSuffix(" %");
    m_tool  = new QLineEdit;
    m_btnConnectRobot = new QPushButton("连接机器人");
    m_lblRobotState = new QLabel("● 未连接"); m_lblRobotState->setObjectName("statusBad");
    fRob->addRow("IP 地址", m_ip);
    fRob->addRow("端口", m_port);
    fRob->addRow("点位运动速度（关节 % / 直线 mm/s）", m_speed);
    fRob->addRow("工具号", m_tool);
    fRob->addRow("", m_btnConnectRobot);
    fRob->addRow("状态", m_lblRobotState);
    grid->addWidget(gbRob, 0, 0);

    // 相机
    auto* gbCam = new QGroupBox("相机 (D435i)");
    auto* fCam = new QFormLayout(gbCam);
    fCam->setSpacing(10); fCam->setContentsMargins(16, 20, 16, 16);
    m_serial = new QLineEdit; m_serial->setPlaceholderText("留空 = 第一台");
    m_w = new QSpinBox; m_w->setRange(320,1920);
    m_h = new QSpinBox; m_h->setRange(240,1080);
    m_fps = new QSpinBox; m_fps->setRange(1,60);
    m_btnOpenCam = new QPushButton("打开相机");
    m_lblCamState = new QLabel("● 未打开"); m_lblCamState->setObjectName("statusBad");
    fCam->addRow("序列号", m_serial);
    fCam->addRow("宽", m_w);
    fCam->addRow("高", m_h);
    fCam->addRow("帧率", m_fps);
    fCam->addRow("", m_btnOpenCam);
    fCam->addRow("状态", m_lblCamState);
    grid->addWidget(gbCam, 0, 1);

    root->addLayout(grid);
    auto* matrixGroup = new QGroupBox("手眼标定矩阵 T_cam2_base");
    auto* matrixLayout = new QHBoxLayout(matrixGroup);
    m_matrixPath = new QLineEdit;
    m_matrixPath->setReadOnly(true);
    auto* chooseMatrix = new QPushButton("选择并加载矩阵");
    matrixLayout->addWidget(m_matrixPath, 1);
    m_matrixStatus = new QLabel(m_calib->hasMatrix() ? "已加载" : "未加载");
    matrixLayout->addWidget(m_matrixStatus);
    matrixLayout->addWidget(chooseMatrix);
    connect(chooseMatrix, &QPushButton::clicked, this, &ConfigPage::onChooseMatrix);
    root->addWidget(matrixGroup);

    auto* btnRow = new QHBoxLayout;
    btnRow->addStretch();
    m_btnSave = new QPushButton("保存配置");
    btnRow->addWidget(m_btnSave);
    root->addLayout(btnRow);
    root->addStretch();

    connect(m_btnConnectRobot, &QPushButton::clicked, this, &ConfigPage::onConnectRobot);
    connect(m_btnOpenCam, &QPushButton::clicked, this, &ConfigPage::onOpenCamera);
    connect(m_btnSave, &QPushButton::clicked, this, &ConfigPage::onSave);
}

void ConfigPage::loadFromCfg()
{
    m_matrixPath->setText(m_cfg->paths.handEyeMatrixFile);
    m_ip->setText(m_cfg->robot.ip);
    m_port->setValue(m_cfg->robot.port);
    m_speed->setValue(m_cfg->robot.speed);
    m_tool->setText(m_cfg->robot.toolName);

    m_serial->setText(m_cfg->camera.serialNumber);
    m_w->setValue(m_cfg->camera.width);
    m_h->setValue(m_cfg->camera.height);
    m_fps->setValue(m_cfg->camera.fps);
}

void ConfigPage::onSave()
{
    m_cfg->robot.ip       = m_ip->text().trimmed();
    m_cfg->robot.port     = m_port->value();
    m_cfg->robot.speed    = m_speed->value();
    m_cfg->robot.toolName = m_tool->text().trimmed();

    m_cfg->camera.serialNumber = m_serial->text().trimmed();
    m_cfg->camera.width  = m_w->value();
    m_cfg->camera.height = m_h->value();
    m_cfg->camera.fps    = m_fps->value();

    m_cfg->ensureDirs();
    QString path = QDir::current().filePath(m_cfg->paths.appConfigFile);
    if (m_cfg->save(path)) emit statusMessage("配置已保存: " + path);
    else                   emit statusMessage("配置保存失败");
}

void ConfigPage::onConnectRobot()
{
    if (m_robot->isConnected()) {
        m_robot->disconnectFrom();
    } else {
        m_cfg->robot.ip = m_ip->text().trimmed();
        m_robot->connectTo(m_cfg->robot.ip, m_cfg->robot.port);
    }
}

void ConfigPage::onOpenCamera()
{
    if (m_cam->isOpened()) {
        m_cam->close();
        m_lblCamState->setText("● 未打开");
        m_lblCamState->setObjectName("statusBad");
        m_btnOpenCam->setText("打开相机");
    } else {
        bool ok = m_cam->open(m_serial->text().trimmed(), m_w->value(), m_h->value(), m_fps->value());
        m_lblCamState->setText(ok ? "● 已打开" : "● 打开失败");
        m_lblCamState->setObjectName(ok ? "statusOk" : "statusBad");
        if (ok) m_btnOpenCam->setText("关闭相机");
    }
    m_lblCamState->style()->unpolish(m_lblCamState);
    m_lblCamState->style()->polish(m_lblCamState);
}

void ConfigPage::onChooseMatrix()
{
    const QString path = QFileDialog::getOpenFileName(this, "选择手眼矩阵", m_matrixPath->text(), "JSON (*.json)");
    if (path.isEmpty()) return;
    if (!m_calib->loadMatrix(path)) { emit statusMessage("矩阵无效，保留原有矩阵"); return; }
    m_matrixPath->setText(path);
    m_matrixStatus->setText("已加载");
    m_cfg->paths.handEyeMatrixFile = path;
    emit statusMessage("手眼矩阵已加载；点击保存配置以便下次启动使用");
}
