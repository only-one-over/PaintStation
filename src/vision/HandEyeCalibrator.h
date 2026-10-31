#pragma once

#include <opencv2/opencv.hpp>
#include <QString>
#include <vector>

#include "RobotController.h"

// 一组采集样本：机器人末端位姿 + 该帧彩色图
struct HandEyeSample {
    int         index = 0;
    qint64      timestampMs = 0;
    qint64      imageTimestampMs = 0;
    ToolPose    robot;
    QString     imageFile;
    cv::Mat     image;
};

// 只做数据采集：每组 = 当前末端位姿 + 一张相机彩色图
// 不在本机做角点检测/求解，外部软件处理。
class HandEyeCalibrator
{
public:
    HandEyeCalibrator();

    void collectSample(const cv::Mat& color, const ToolPose& robotTool, qint64 imageTimestampMs);

    int  sampleCount() const { return static_cast<int>(m_samples.size()); }
    const std::vector<HandEyeSample>& samples() const { return m_samples; }
    void clearSamples();

    bool exportDataset(const QString& jsonFile, const QString& imageDir) const;

    // ---- 外部求解好的 T_cam2_base 导入（用于验证页坐标变换） ----
    bool hasMatrix() const { return m_haveMatrix; }
    cv::Point3f camToBase(const cv::Point3f& p_cam) const;
    bool loadMatrix(const QString& file);

private:
    std::vector<HandEyeSample> m_samples;

    cv::Matx44d m_T_cam2_base{1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};
    bool m_haveMatrix = false;
};
