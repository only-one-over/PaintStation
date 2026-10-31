#include "RobotController.h"

#include <EfortSdk.h>
#include <SdkStructDef.h>

#include <QDebug>

using namespace RobotAPI;

RobotController::RobotController(QObject* parent) : QObject(parent) {}

RobotController::~RobotController()
{
    disconnectFrom();
}

bool RobotController::connectTo(const QString& ip, int /*port*/)
{
    std::lock_guard<std::mutex> lk(m_mtx);
    if (m_connected) return true;

    unsigned int dev = 0;
    // 埃夫特 SDK 通过 IP 直连，端口在内部协议里固定
    int rc = ConnectRobot(ip.toStdString(), dev, false, false, 3, true);
    if (rc != 0) {
        emit connectionChanged(false, QString("ConnectRobot failed, rc=%1").arg(rc));
        return false;
    }
    m_devId = dev;
    m_connected = true;

    // 切到远程 API 控制
    EnableApiControl(true, m_devId);

    emit connectionChanged(true, QString("connected %1").arg(ip));
    qInfo() << "[Robot] connected, devId=" << m_devId;
    return true;
}

void RobotController::disconnectFrom()
{
    std::lock_guard<std::mutex> lk(m_mtx);
    if (!m_connected) return;
    EnableApiControl(false, m_devId);
    DisconnectRobot(m_devId);
    m_connected = false;
    m_devId = 0;
    emit connectionChanged(false, "disconnected");
}

bool RobotController::isConnected() const { return m_connected; }

bool RobotController::enableApi(bool en)
{
    if (!m_connected) return false;
    int rc = EnableApiControl(en, m_devId);
    return rc == 0;
}

bool RobotController::getCurrentToolPose(ToolPose& out)
{
    if (!m_connected) return false;
    RobotPos pos;
    int rc = GetBaseCoordinatePos(pos, m_devId);
    if (rc != 0) return false;
    out.x = pos.x; out.y = pos.y; out.z = pos.z;
    out.a = pos.a; out.b = pos.b; out.c = pos.c;
    return true;
}

void RobotController::estop()
{
    if (!m_connected) return;
    MOVEHOLD(m_devId);
}

void RobotController::clearAlarm()
{
    if (!m_connected) return;
    ClearAlarm(m_devId);
}

bool RobotController::moveJoint(const double j[6], double speedPercent)
{
    if (!m_connected) return false;
    double pos[6] = {j[0],j[1],j[2],j[3],j[4],j[5]};
    int rc = MJOINT(pos, static_cast<int>(speedPercent), m_devId);
    return rc == 0;
}

bool RobotController::moveCartesian(const ToolPose& p, double speedPercent)
{
    if (!m_connected) return false;
    double pos[6] = {p.x, p.y, p.z, p.a, p.b, p.c};
    int rc = MLIN(pos, static_cast<int>(speedPercent), m_devId);
    return rc == 0;
}

bool RobotController::stopMotion()
{
    if (!m_connected) return false;
    int rc = MOVEHOLD(m_devId);
    return rc == 0;
}

bool RobotController::writeDO(int index, bool on)
{
    if (!m_connected) return false;
    int rc = WriteDOut(static_cast<unsigned int>(index), on, m_devId);
    return rc == 0;
}
