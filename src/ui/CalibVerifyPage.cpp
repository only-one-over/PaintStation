#include "CalibVerifyPage.h"
#include "ImageWidget.h"
#include "AppConfig.h"
#include "RobotController.h"
#include "Camera.h"
#include "HandEyeCalibrator.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QFileDialog>
#include <QFileInfo>
#include <QDir>
#include <QDateTime>
#include <QDebug>
#include <QThread>

static QImage cvMatToQImage(const cv::Mat& src)
{
    if (src.type() == CV_8UC3) {
        return QImage(src.data, src.cols, src.rows, (int)src.step, QImage::Format_BGR888).copy();
    }
    return QImage();
}

CalibVerifyPage::CalibVerifyPage(AppConfig* cfg, RobotController* robot, Camera* cam,
                                 HandEyeCalibrator* calib, QWidget* parent)
    : QWidget(parent), m_cfg(cfg), m_robot(robot), m_cam(cam), m_calib(calib)
{
    buildUi();
    m_timer.setInterval(33); // ~30fps
    connect(&m_timer, &QTimer::timeout, this, &CalibVerifyPage::onTimeout);
    m_timer.start();
}

void CalibVerifyPage::buildUi()
{
    auto* root = new QHBoxLayout(this);
    root->setContentsMargins(16, 16, 16, 16);
    root->setSpacing(16);

    m_view = new ImageWidget;
    root->addWidget(m_view, 1);

    auto* side = new QVBoxLayout;
    side->setSpacing(12);
    side->setContentsMargins(0, 0, 0, 0);

    auto* title = new QLabel("手眼标定验证");
    title->setObjectName("titleLabel");
    side->addWidget(title);

    m_lblInfo = new QLabel("点击左侧图像任意点\n查看 3D 坐标");
    m_lblInfo->setObjectName("coordLabel");
    m_lblInfo->setMinimumWidth(280);
    m_lblInfo->setMinimumHeight(180);
    side->addWidget(m_lblInfo);

    m_lblSampleCount = new QLabel("已采集: 0 组");
    side->addWidget(m_lblSampleCount);

    m_btnCollect = new QPushButton("采集一组 (棋盘格)");
    m_btnExport  = new QPushButton("导出数据集 (JSON+图)");
    m_btnClear   = new QPushButton("清空采集");
    m_btnLoad    = new QPushButton("导入外部矩阵");
    m_btnClear->setProperty("secondary", true);
    m_btnLoad->setProperty("secondary", true);
    side->addWidget(m_btnCollect);
    side->addWidget(m_btnExport);
    side->addWidget(m_btnClear);
    side->addWidget(m_btnLoad);
    side->addStretch();
    root->addLayout(side);

    connect(m_view, &ImageWidget::pixelClicked, this, &CalibVerifyPage::onPixelClicked);
    connect(m_btnCollect, &QPushButton::clicked, this, &CalibVerifyPage::onCollectSample);
    connect(m_btnExport,  &QPushButton::clicked, this, &CalibVerifyPage::onExportDataset);
    connect(m_btnClear,   &QPushButton::clicked, this, &CalibVerifyPage::onClearSamples);
    connect(m_btnLoad,    &QPushButton::clicked, this, &CalibVerifyPage::onLoadMatrix);
}

void CalibVerifyPage::onTimeout()
{
    if (!m_cam || !m_cam->isOpened()) return;
    cv::Mat color, depth;
    if (!m_cam->readFrame(color, depth)) return;
    m_lastColor = color;
    m_lastDepthMM = depth;
    m_view->showImage(cvMatToQImage(color));
}

void CalibVerifyPage::onPixelClicked(int u, int v)
{
    if (m_lastColor.empty()) return;

    // 深度
    if (m_lastDepthMM.empty() || u < 0 || v < 0 || u >= m_lastDepthMM.cols || v >= m_lastDepthMM.rows) return;
    uint16_t zMM = m_lastDepthMM.at<uint16_t>(v, u);

    Camera::Intrinsics K = m_cam->intrinsics();
    if (zMM == 0 || K.fx <= 0 || K.fy <= 0) {
        m_lblInfo->setText("该像素深度或相机内参无效");
        return;
    }
    cv::Point3f p_cam = m_cam->pixelToCamera(u, v, (float)zMM, K);
    if (!m_calib->hasMatrix()) {
        m_lblInfo->setText(QString("像素: (%1, %2)\n深度: %3 mm\n相机坐标: (%4, %5, %6) mm\n尚未加载手眼矩阵")
            .arg(u).arg(v).arg(zMM).arg(p_cam.x, 0, 'f', 1).arg(p_cam.y, 0, 'f', 1).arg(p_cam.z, 0, 'f', 1));
        return;
    }
    cv::Point3f p_base = m_calib->camToBase(p_cam);

    QString txt = QString(
        "像素: (%1, %2)\n"
        "深度 z: %3 mm\n"
        "--- 相机坐标系 ---\n"
        "Xc: %4 mm\nYc: %5 mm\nZc: %6 mm\n"
        "--- 基坐标系 (手眼矩阵) ---\n"
        "Xb: %7 mm\nYb: %8 mm\nZb: %9 mm\n"
        "手眼矩阵: %10"
    ).arg(u).arg(v)
     .arg(zMM)
     .arg(p_cam.x, 0, 'f', 1).arg(p_cam.y, 0, 'f', 1).arg(p_cam.z, 0, 'f', 1)
     .arg(p_base.x, 0, 'f', 1).arg(p_base.y, 0, 'f', 1).arg(p_base.z, 0, 'f', 1)
     .arg(m_calib->hasMatrix() ? "已加载" : "未标定(相机坐标=基坐标)");
    m_lblInfo->setText(txt);
}

void CalibVerifyPage::onCollectSample()
{
    if (m_lastColor.empty()) { emit statusMessage("相机未出图"); return; }
    ToolPose p;
    if (!m_robot->getCurrentToolPose(p)) {
        emit statusMessage("读不到机器人位姿，请先在配置页连接机器人");
        return;
    }
    m_calib->collectSample(m_lastColor, p);
    m_lblSampleCount->setText(QString("已采集: %1 组").arg(m_calib->sampleCount()));
    emit statusMessage(QString("采集成功 #%1  (x=%2 y=%3 z=%4)")
        .arg(m_calib->sampleCount()).arg(p.x,0,'f',1).arg(p.y,0,'f',1).arg(p.z,0,'f',1));
}

void CalibVerifyPage::onExportDataset()
{
    if (m_calib->sampleCount() == 0) {
        emit statusMessage("还没采集任何样本");
        return;
    }
    // 默认到 data/calibration/sessions/<时间戳>/
    QString stamp = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss");
    QString dir = QFileDialog::getExistingDirectory(this, "选择数据集保存目录",
        QDir::current().filePath(m_cfg->paths.calibSessionsDir + "/" + stamp));
    if (dir.isEmpty()) return;
    QDir().mkpath(dir);
    QString jsonPath = QDir(dir).filePath("handeye_dataset.json");
    QString imgDir   = QDir(dir).filePath("images");
    if (m_calib->exportDataset(jsonPath, imgDir)) {
        emit statusMessage(QString("已导出 %1 组 -> %2").arg(m_calib->sampleCount()).arg(jsonPath));
    } else {
        emit statusMessage("导出失败");
    }
}

void CalibVerifyPage::onClearSamples()
{
    m_calib->clearSamples();
    m_lblSampleCount->setText("已采集: 0 组");
    emit statusMessage("已清空采集");
}

void CalibVerifyPage::onLoadMatrix()
{
    QString path = QFileDialog::getOpenFileName(this, "选择外部求解好的 T_cam2_base JSON",
        QDir::current().filePath(m_cfg->paths.handEyeMatrixFile), "JSON (*.json)");
    if (path.isEmpty()) return;
    if (m_calib->loadMatrix(path)) emit statusMessage("已加载矩阵: " + path);
    else                           emit statusMessage("加载失败");
}
