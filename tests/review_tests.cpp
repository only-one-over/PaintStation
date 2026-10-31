#include "HandEyeCalibrator.h"
#include "PointStore.h"
#include "FlowStore.h"
#include <QCoreApplication>
#include <QTemporaryDir>
#include <QFile>
#include <QDir>
#include <cmath>
#include <QJsonDocument>
#include <QJsonObject>
#include <cassert>

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    QTemporaryDir dir(QDir::current().filePath("review-tests-XXXXXX"));
    assert(dir.isValid());
    auto write = [](const QString& path, const QByteArray& data) {
        QFile f(path); assert(f.open(QIODevice::WriteOnly)); assert(f.write(data) == data.size());
    };
    const QString matrixFile = dir.filePath("matrix.json");
    QJsonObject matrix;
    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j)
            matrix[QString("r%1c%2").arg(i).arg(j)] = i == j ? 1.0 : 0.0;
    matrix["r0c3"] = 10.0;
    QJsonObject root{{"hasMatrix", true}, {"T_cam2_base", matrix}};
    write(matrixFile, QJsonDocument(root).toJson());
    HandEyeCalibrator calibrator;
    assert(calibrator.loadMatrix(matrixFile));
    assert(std::abs(calibrator.camToBase({1,2,3}).x - 11) < 0.001);
    matrix.remove("r0c0");
    root["T_cam2_base"] = matrix;
    write(matrixFile, QJsonDocument(root).toJson());
    assert(!calibrator.loadMatrix(matrixFile));
    assert(std::abs(calibrator.camToBase({1,2,3}).x - 11) < 0.001);
    const QString pointsFile = dir.filePath("points.json");
    PointStore points;
    RobotPoint point; point.name = "P1"; points.points.append(point);
    assert(points.save(pointsFile));
    PointStore loaded;
    assert(loaded.load(pointsFile) && loaded.points.size() == 1);
    write(pointsFile, "invalid json");
    assert(!loaded.load(pointsFile) && loaded.points.size() == 1);
    const QString flowFile = dir.filePath("flow.json");
    FlowStore flows; Flow flow; flow.name = "test";
    assert(flows.save(flowFile, flow));
    Flow loadedFlow; assert(flows.load(flowFile, loadedFlow) && loadedFlow.name == "test");
    write(flowFile, "{}");
    assert(!flows.load(flowFile, loadedFlow) && loadedFlow.name == "test");
}
