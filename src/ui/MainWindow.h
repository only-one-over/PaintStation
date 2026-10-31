#pragma once

#include <QMainWindow>
#include <memory>
#include <QTimer>

class QListWidget;
class QStackedWidget;
class QLabel;
class AppConfig;
class RobotController;
class Camera;
class HandEyeCalibrator;
class PointStore;

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow();

private:
    void buildUi();
    void refreshStatusBar();

    QListWidget*    m_nav = nullptr;
    QStackedWidget* m_stack = nullptr;
    QLabel*     m_lblRobotState = nullptr;
    QLabel*     m_lblCamState    = nullptr;
    QLabel*     m_lblPose       = nullptr;
    QTimer      m_statusTimer;

    std::unique_ptr<AppConfig>         m_cfg;
    std::unique_ptr<RobotController>   m_robot;
    std::unique_ptr<Camera>            m_cam;
    std::unique_ptr<HandEyeCalibrator> m_calib;
    std::unique_ptr<PointStore>       m_points;
};
