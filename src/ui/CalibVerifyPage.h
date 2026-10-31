#pragma once

#include <QWidget>
#include <QTimer>
#include <opencv2/opencv.hpp>

class ImageWidget;
class QLabel;
class QPushButton;
class QLineEdit;
class QSpinBox;

class AppConfig;
class RobotController;
class Camera;
class HandEyeCalibrator;

// 第 2 页：手眼标定验证
//   - 实时显示 D435i 彩色图 + 对齐深度
//   - 鼠标点一下像素 -> 得到该点在相机坐标系和机器人基坐标系下的 3D 坐标
//   - 兼做 eye-to-hand 标定流程：采集 N 组 -> 求解 T_cam2_base
class CalibVerifyPage : public QWidget
{
    Q_OBJECT
public:
    CalibVerifyPage(AppConfig* cfg, RobotController* robot, Camera* cam,
                    HandEyeCalibrator* calib, QWidget* parent = nullptr);

signals:
    void statusMessage(const QString&);

private slots:
    void onTimeout();
    void onPixelClicked(int u, int v);
    void onCollectSample();
    void onExportDataset();     // 导出规范 JSON + 图像
    void onLoadMatrix();
    void onClearSamples();      // 清空已采集

private:
    void buildUi();
    cv::Mat m_lastColor;
    cv::Mat m_lastDepthMM;

    AppConfig*        m_cfg;
    RobotController* m_robot;
    Camera*          m_cam;
    HandEyeCalibrator* m_calib;

    QTimer  m_timer;
    ImageWidget* m_view;
    QLabel*  m_lblInfo;
    QLabel*  m_lblSampleCount;
    QPushButton* m_btnCollect;
    QPushButton* m_btnExport;
    QPushButton* m_btnClear;
    QPushButton* m_btnLoad;
};
