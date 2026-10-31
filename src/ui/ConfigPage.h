#pragma once

#include <QWidget>

class QLineEdit;
class QSpinBox;
class QDoubleSpinBox;
class QPushButton;
class QLabel;

class AppConfig;
class RobotController;
class Camera;
class HandEyeCalibrator;

class ConfigPage : public QWidget
{
    Q_OBJECT
public:
    ConfigPage(AppConfig* cfg, RobotController* robot, Camera* cam, HandEyeCalibrator* calib, QWidget* parent = nullptr);

signals:
    void statusMessage(const QString&);

private slots:
    void onSave();
    void onConnectRobot();
    void onOpenCamera();
    void onChooseMatrix();

private:
    void buildUi();
    void loadFromCfg();

    AppConfig*     m_cfg;
    RobotController* m_robot;
    Camera*        m_cam;
    HandEyeCalibrator* m_calib;
    QLineEdit* m_matrixPath;
    QLabel* m_matrixStatus;

    QLineEdit* m_ip;      QSpinBox* m_port;
    QSpinBox*  m_speed;   QLineEdit* m_tool;
    QPushButton* m_btnConnectRobot;
    QLabel*      m_lblRobotState;

    QLineEdit* m_serial;
    QSpinBox*  m_w; QSpinBox* m_h; QSpinBox* m_fps;
    QPushButton* m_btnOpenCam;
    QLabel*      m_lblCamState;

    QPushButton* m_btnSave;
};
