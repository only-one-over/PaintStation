#pragma once

#include "Camera.h"
#include <QString>

#if HAVE_LIBREALSENSE
#include <librealsense2/rs.hpp>
#endif

class RealSenseCamera : public Camera
{
public:
    RealSenseCamera();
    ~RealSenseCamera() override;

    bool open(const QString& serial, int width, int height, int fps) override;
    void close() override;
    bool isOpened() const override;

    bool readFrame(cv::Mat& color, cv::Mat& alignedDepth) override;
    Intrinsics intrinsics() const override { return m_K; }

private:
#if HAVE_LIBREALSENSE
    rs2::pipeline*   m_pipe = nullptr;
    rs2::align       m_align{RS2_STREAM_COLOR};
#endif
    bool             m_opened = false;
    Intrinsics       m_K;
};
