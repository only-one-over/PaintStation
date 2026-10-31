#include "RobotController.h"

#include <EfortSdk.h>
#include <SdkStructDef.h>
#include <SdkConstDef.h>

#include <QDebug>
#include <QElapsedTimer>
#include <QThread>
#include <cmath>

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
    if (EnableApiControl(true, m_devId) != 0) {
        DisconnectRobot(m_devId);
        m_connected = false;
        m_devId = 0;
        emit connectionChanged(false, "启用 API 控制失败");
        return false;
    }

    emit connectionChanged(true, QString("connected %1").arg(ip));
    qInfo() << "[Robot] connected, devId=" << m_devId;
    return true;
}

void RobotController::disconnectFrom()
{
    std::lock_guard<std::mutex> lk(m_mtx);
    if (!m_connected || m_flowActive) return;
    EnableApiControl(false, m_devId);
    DisconnectRobot(m_devId);
    m_connected = false;
    m_devId = 0;
    emit connectionChanged(false, "disconnected");
}

bool RobotController::isConnected() const { std::lock_guard<std::mutex> lk(m_mtx); return m_connected; }

bool RobotController::enableApi(bool en)
{
    std::lock_guard<std::mutex> lk(m_mtx);
    if (!m_connected) return false;
    int rc = EnableApiControl(en, m_devId);
    return rc == 0;
}

bool RobotController::getCurrentToolPose(ToolPose& out)
{
    std::lock_guard<std::mutex> lk(m_mtx);
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
    stopMotion();
}

void RobotController::clearAlarm()
{
    std::lock_guard<std::mutex> lk(m_mtx);
    if (!m_connected) return;
    ClearAlarm(m_devId);
}

bool RobotController::beginFlow()
{
    std::lock_guard<std::mutex> lk(m_mtx);
    if (!m_connected || m_flowActive) return false;
    m_stopRequested = false;
    m_flowActive = true;
    return true;
}

void RobotController::endFlow()
{
    std::lock_guard<std::mutex> lk(m_mtx);
    m_flowActive = false;
}

bool RobotController::moveJoint(const double j[6], double speed, const QString& tool)
{
    RobotJoint target{};
    double pos[6];
    for (int i = 0; i < 6; ++i) { pos[i] = j[i]; target.j[i] = j[i]; }
    {
        std::lock_guard<std::mutex> lk(m_mtx);
        std::string currentTool;
        if (!m_connected || !m_flowActive || m_stopRequested || speed < 1 || speed > 100 ||
            GetCurrentToolName(currentTool, m_devId) != 0 ||
            QString::fromStdString(currentTool).compare(tool, Qt::CaseInsensitive) != 0 ||
            MJOINT(pos, static_cast<int>(speed), m_devId) != 0) return false;
    }
    QElapsedTimer timer; timer.start();
    while (timer.elapsed() < 120000) {
        {
            std::lock_guard<std::mutex> lk(m_mtx);
            if (!m_connected || m_stopRequested) return false;
            const int rc = WaitJointPosition(target, 0.05, m_devId, 0.2);
            if (rc == 0) return true;
            if (rc != ERROR_ROBOT_NOT_IN_POSITION && rc != ERROR_THIRD_OPERATION_TIME_OUT) {
                m_stopRequested = true;
                MOVEHOLD(m_devId);
                return false;
            }
        }
        QThread::msleep(20);
    }
    stopMotion();
    return false;
}

bool RobotController::moveCartesian(const ToolPose& p, double speed, const QString& tool)
{
    double pos[6] = {p.x, p.y, p.z, p.a, p.b, p.c};
    RobotPos target{};
    target.x=p.x; target.y=p.y; target.z=p.z; target.a=p.a; target.b=p.b; target.c=p.c;
    {
        std::lock_guard<std::mutex> lk(m_mtx);
        std::string currentTool;
        if (!m_connected || !m_flowActive || m_stopRequested || speed < 1 || speed > 100 ||
            GetCurrentToolName(currentTool, m_devId) != 0 ||
            QString::fromStdString(currentTool).compare(tool, Qt::CaseInsensitive) != 0 ||
            MLIN(pos, static_cast<int>(speed), m_devId) != 0) return false;
    }
    QElapsedTimer timer; timer.start();
    while (timer.elapsed() < 120000) {
        {
            std::lock_guard<std::mutex> lk(m_mtx);
            if (!m_connected || m_stopRequested) return false;
            const int rc = WaitCartPosition(target, 0.03, m_devId, 0.2);
            if (rc == 0) return true;
            if (rc != ERROR_ROBOT_NOT_IN_POSITION && rc != ERROR_THIRD_OPERATION_TIME_OUT) {
                m_stopRequested = true;
                MOVEHOLD(m_devId);
                return false;
            }
        }
        QThread::msleep(20);
    }
    stopMotion();
    return false;
}

bool RobotController::stopMotion()
{
    m_stopRequested = true;
    std::lock_guard<std::mutex> lk(m_mtx);
    return m_connected && MOVEHOLD(m_devId) == 0;
}

bool RobotController::writeDO(int index, bool on)
{
    std::lock_guard<std::mutex> lk(m_mtx);
    if (!m_connected || !m_flowActive || m_stopRequested || index < 0 || index > 255) return false;
    return WriteDOut(static_cast<unsigned int>(index), on, m_devId) == 0;
}
