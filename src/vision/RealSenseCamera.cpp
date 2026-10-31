#include "RealSenseCamera.h"

#include <QDebug>
#include <memory>
#include <cmath>
#if HAVE_LIBREALSENSE
#include <librealsense2/rsutil.h>
#endif

RealSenseCamera::RealSenseCamera() = default;
RealSenseCamera::~RealSenseCamera() { close(); }

#if HAVE_LIBREALSENSE

bool RealSenseCamera::open(const QString& serial, int width, int height, int fps)
{
    close();
    try {
        auto pipe = std::make_unique<rs2::pipeline>();
        rs2::config cfg;
        if (!serial.isEmpty()) cfg.enable_device(serial.toStdString());
        cfg.enable_stream(RS2_STREAM_COLOR, width, height, RS2_FORMAT_BGR8, fps);
        cfg.enable_stream(RS2_STREAM_DEPTH, width, height, RS2_FORMAT_Z16, fps);
        rs2::pipeline_profile prof = pipe->start(cfg);

        auto intr = prof.get_stream(RS2_STREAM_COLOR)
                        .as<rs2::video_stream_profile>().get_intrinsics();
        m_colorIntrinsics = intr;
        m_K.width = intr.width; m_K.height = intr.height;
        m_K.fx = intr.fx;      m_K.fy = intr.fy;
        m_K.cx = intr.ppx;     m_K.cy = intr.ppy;
        for (int i = 0; i < 5; ++i) m_K.coeffs[i] = intr.coeffs[i];

        m_pipe = pipe.release();
        m_opened = true;
        qInfo() << "[RealSense] opened" << (serial.isEmpty()?"<default>":serial);
        return true;
    } catch (const rs2::error& e) {
        qWarning() << "[RealSense] open failed:" << e.what();
        close();
        return false;
    }
}

void RealSenseCamera::close()
{
    if (m_pipe) {
        try { m_pipe->stop(); } catch (...) {}
        delete m_pipe;
        m_pipe = nullptr;
    }
    m_opened = false;
}

bool RealSenseCamera::isOpened() const { return m_pipe != nullptr; }

bool RealSenseCamera::readFrame(cv::Mat& color, cv::Mat& alignedDepth)
{
    if (!m_pipe) return false;
    try {
    rs2::frameset frames = m_pipe->wait_for_frames(100);
    rs2::frameset aligned = m_align.process(frames);

    auto cf = aligned.get_color_frame();
    auto df = aligned.get_depth_frame();
    if (!cf || !df) return false;

    color = cv::Mat(cv::Size(cf.get_width(), cf.get_height()), CV_8UC3,
                    (void*)cf.get_data(), cv::Mat::AUTO_STEP).clone();

    const uint16_t* d = (const uint16_t*)df.get_data();
    int w = df.get_width(), h = df.get_height();
    alignedDepth = cv::Mat(cv::Size(w, h), CV_16U);
    float scale = df.get_units() * 1000.0f;
    for (int i = 0; i < w*h; ++i)
        alignedDepth.at<uint16_t>(i) = static_cast<uint16_t>(d[i] * scale);
    return true;
    } catch (const rs2::error& e) {
        qWarning() << "[RealSense] frame failed:" << e.what();
        return false;
    }
}

bool RealSenseCamera::pixelToCamera(int u, int v, float zMM, cv::Point3f& out) const
{
    if (!m_pipe || zMM <= 0 || m_colorIntrinsics.fx <= 0 || m_colorIntrinsics.fy <= 0 ||
        m_colorIntrinsics.model == RS2_DISTORTION_MODIFIED_BROWN_CONRADY) return false;
    float pixel[2] = {static_cast<float>(u), static_cast<float>(v)};
    float point[3];
    rs2_deproject_pixel_to_point(point, &m_colorIntrinsics, pixel, zMM);
    out = {point[0], point[1], point[2]};
    return std::isfinite(point[0]) && std::isfinite(point[1]) && std::isfinite(point[2]);
}

#else  // ---- STUB: 未装 librealsense2 ----

bool RealSenseCamera::open(const QString&, int, int, int)
{
    qWarning() << "[RealSense] STUB build - librealsense2 not installed. Camera does nothing.";
    return false;
}
void RealSenseCamera::close() { m_opened = false; }
bool RealSenseCamera::isOpened() const { return m_opened; }
bool RealSenseCamera::readFrame(cv::Mat&, cv::Mat&) { return false; }
bool RealSenseCamera::pixelToCamera(int, int, float, cv::Point3f&) const { return false; }

#endif
