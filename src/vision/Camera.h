#pragma once

#include <opencv2/opencv.hpp>
#include <QString>

// 相机抽象接口。第一版只用 RealSenseCamera 实现；
// 后续接其它相机只需要继承这个接口，UI/Robot 层不用动。
class Camera {
public:
    virtual ~Camera() = default;

    virtual bool open(const QString& serial, int width, int height, int fps) = 0;
    virtual void close() = 0;
    virtual bool isOpened() const = 0;

    // 取一帧：color=BGR (OpenCV格式)，alignedDepth=与彩色对齐后的深度图（单位 mm，CV_16U）
    // 成功返回 true
    virtual bool readFrame(cv::Mat& color, cv::Mat& alignedDepth) = 0;

    // 彩色相机内参
    struct Intrinsics {
        int width = 0, height = 0;
        double fx = 0, fy = 0, cx = 0, cy = 0;
        double coeffs[5] = {0};
    };
    virtual Intrinsics intrinsics() const = 0;

    // 像素(u,v) + 对应深度 z(mm) -> 相机坐标系下 3D 点 (mm)
    virtual bool pixelToCamera(int u, int v, float zMM, cv::Point3f& out) const = 0;
};
