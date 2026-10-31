#include "HandEyeCalibrator.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDir>
#include <QFileInfo>
#include <QDateTime>
#include <QDebug>
#include <cmath>

HandEyeCalibrator::HandEyeCalibrator() = default;

void HandEyeCalibrator::collectSample(const cv::Mat& color, const ToolPose& pose)
{
    HandEyeSample s;
    s.index       = static_cast<int>(m_samples.size());
    s.timestampMs = QDateTime::currentMSecsSinceEpoch();
    s.robot       = pose;
    s.image       = color.clone();
    m_samples.push_back(std::move(s));
}

void HandEyeCalibrator::clearSamples()
{
    m_samples.clear();
}

bool HandEyeCalibrator::exportDataset(const QString& jsonFile, const QString& imageDir) const
{
    QDir().mkpath(QFileInfo(jsonFile).absolutePath());
    QDir().mkpath(imageDir);

    QJsonArray arr;
    for (const auto& s : m_samples) {
        QString imgName = QString("sample_%1.png").arg(s.index, 3, 10, QChar('0'));
        cv::imwrite(QDir(imageDir).filePath(imgName).toStdString(), s.image);

        QJsonObject o;
        o["index"]     = s.index;
        o["timestamp"] = s.timestampMs;
        QJsonObject rp;
        rp["x"]=s.robot.x; rp["y"]=s.robot.y; rp["z"]=s.robot.z;
        rp["a"]=s.robot.a; rp["b"]=s.robot.b; rp["c"]=s.robot.c;
        o["robot_pose"]=rp;
        o["image_file"]=imgName;
        arr.append(o);
    }

    QJsonObject root;
    root["created_at"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    root["sample_count"] = static_cast<int>(m_samples.size());
    root["samples"] = arr;

    QFile f(jsonFile);
    if (!f.open(QIODevice::WriteOnly)) return false;
    f.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    qInfo() << "[HandEye] exported" << m_samples.size() << "samples to" << jsonFile;
    return true;
}

cv::Point3f HandEyeCalibrator::camToBase(const cv::Point3f& p_cam) const
{
    if (!m_haveMatrix) return p_cam;
    cv::Matx31d p(p_cam.x/1000.0, p_cam.y/1000.0, p_cam.z/1000.0);
    cv::Matx33d R(m_T_cam2_base(0,0),m_T_cam2_base(0,1),m_T_cam2_base(0,2),
                  m_T_cam2_base(1,0),m_T_cam2_base(1,1),m_T_cam2_base(1,2),
                  m_T_cam2_base(2,0),m_T_cam2_base(2,1),m_T_cam2_base(2,2));
    cv::Matx31d t(m_T_cam2_base(0,3)/1000.0, m_T_cam2_base(1,3)/1000.0, m_T_cam2_base(2,3)/1000.0);
    cv::Matx31d p_b = R*p + t;
    return {static_cast<float>(p_b(0)*1000.0),
            static_cast<float>(p_b(1)*1000.0),
            static_cast<float>(p_b(2)*1000.0)};
}

bool HandEyeCalibrator::loadMatrix(const QString& file)
{
    QFile f(file);
    if (!f.open(QIODevice::ReadOnly)) return false;
    const auto doc = QJsonDocument::fromJson(f.readAll());
    if (!doc.isObject()) return false;
    const auto o = doc.object();
    if (!o.value("hasMatrix").toBool() || !o.value("T_cam2_base").isObject()) return false;
    const auto T = o.value("T_cam2_base").toObject();
    cv::Matx44d candidate;
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            const auto value = T.value(QString("r%1c%2").arg(i).arg(j));
            if (!value.isDouble() || !std::isfinite(value.toDouble())) return false;
            candidate(i,j) = value.toDouble();
        }
    }
    for (int j = 0; j < 4; ++j)
        if (std::abs(candidate(3,j) - (j == 3 ? 1.0 : 0.0)) > 1e-6) return false;
    cv::Matx33d R(candidate(0,0), candidate(0,1), candidate(0,2),
                  candidate(1,0), candidate(1,1), candidate(1,2),
                  candidate(2,0), candidate(2,1), candidate(2,2));
    if (cv::norm(cv::Mat(R.t() * R - cv::Matx33d::eye())) > 1e-3 ||
        std::abs(cv::determinant(cv::Mat(R)) - 1.0) > 1e-3) return false;
    m_T_cam2_base = candidate;
    m_haveMatrix = true;
    return true;
}
