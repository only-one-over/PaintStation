#pragma once

#include <QObject>
#include <QString>
#include <mutex>
#include <atomic>

// 末端位姿（工具坐标系原点在基坐标系下）
// x,y,z 单位 mm，姿态单位为度；旋转约定以控制器配置为准。
struct ToolPose {
    double x = 0, y = 0, z = 0;
    double a = 0, b = 0, c = 0;
};

class RobotController : public QObject
{
    Q_OBJECT
public:
    explicit RobotController(QObject* parent = nullptr);
    ~RobotController();

    bool connectTo(const QString& ip, int port);
    void disconnectFrom();
    bool isConnected() const;

    // 开启/关闭 SDK 伺服权限（EnableApiControl）
    bool enableApi(bool en);

    // 读当前末端位姿；失败返回 false
    bool getCurrentToolPose(ToolPose& out);

    // 运动接口（阻塞直到运动完成）
    bool beginFlow();
    void endFlow();
    bool moveJoint(const double j[6], double speedPercent, const QString& tool);
    bool moveCartesian(const ToolPose& p, double speed, const QString& tool);
    bool stopMotion();

    // IO
    bool writeDO(int index, bool on);

    // 急停/清报警
    void estop();
    void clearAlarm();

signals:
    void connectionChanged(bool connected, const QString& msg);

private:
    mutable std::mutex m_mtx;
    bool m_flowActive = false;
    std::atomic<bool> m_stopRequested{false};
    unsigned int m_devId = 0;
    bool m_connected = false;
};
