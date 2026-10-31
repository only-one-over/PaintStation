#include "HandEyeCalibrator.h"
#include "PointStore.h"
#include "AppConfig.h"
#include "FlowStore.h"
#include <QCoreApplication>
#include <QTemporaryDir>
#include <QFile>
#include <QDir>
#include <cmath>
#include <QJsonDocument>
#include <QJsonObject>
#include <cstdlib>
#include <QDebug>
#define CHECK(expr) do { if (!(expr)) { qCritical() << "Check failed:" << #expr << __LINE__; return 1; } } while (false)

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    QTemporaryDir dir(QDir::current().filePath("review-tests-XXXXXX"));
    CHECK(dir.isValid());
    auto write = [](const QString& path, const QByteArray& data) {
        QFile f(path); return f.open(QIODevice::WriteOnly) && f.write(data) == data.size();
    };
    const QString matrixFile = dir.filePath("matrix.json");
    QJsonObject matrix;
    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j)
            matrix[QString("r%1c%2").arg(i).arg(j)] = i == j ? 1.0 : 0.0;
    matrix["r0c3"] = 10.0;
    QJsonObject root{{"hasMatrix", true}, {"T_cam2_base", matrix}};
    CHECK(write(matrixFile, QJsonDocument(root).toJson()));
    HandEyeCalibrator calibrator;
    CHECK(calibrator.loadMatrix(matrixFile));
    CHECK(std::abs(calibrator.camToBase({1,2,3}).x - 11) < 0.001);
    matrix.remove("r0c0");
    root["T_cam2_base"] = matrix;
    CHECK(write(matrixFile, QJsonDocument(root).toJson()));
    CHECK(!calibrator.loadMatrix(matrixFile));
    CHECK(std::abs(calibrator.camToBase({1,2,3}).x - 11) < 0.001);
    cv::Mat image(4, 4, CV_8UC3, cv::Scalar(1, 2, 3));
    calibrator.collectSample(image, ToolPose{}, 1234);
    const auto dataset = dir.filePath("dataset.json");
    CHECK(calibrator.exportDataset(dataset, dir.filePath("images")));
    CHECK(QFile::exists(dir.filePath("images/sample_000.png")));
    QFile exported(dataset); CHECK(exported.open(QIODevice::ReadOnly));
    const auto exportedRoot = QJsonDocument::fromJson(exported.readAll()).object();
    CHECK(exportedRoot.value("sample_count").toInt() == 1);
    CHECK(!calibrator.exportDataset(dir.filePath("missing/sub/dataset.json"), dir.filePath("images/sample_000.png/invalid")));
    const QString configFile = dir.filePath("config.json");
    AppConfig config; config.paths.handEyeMatrixFile = matrixFile; config.robot.speed = 35;
    CHECK(config.save(configFile));
    AppConfig loadedConfig; CHECK(loadedConfig.load(configFile));
    CHECK(loadedConfig.paths.handEyeMatrixFile == matrixFile && loadedConfig.robot.speed == 35);
    const QString pointsFile = dir.filePath("points.json");
    PointStore points;
    RobotPoint point; point.name = "P1"; point.j[0] = 42.5; points.points.append(point);
    CHECK(points.save(pointsFile));
    PointStore loaded;
    CHECK(loaded.load(pointsFile) && loaded.points.size() == 1 && loaded.points[0].j[0] == 42.5);
    CHECK(write(pointsFile, "invalid json"));
    CHECK(!loaded.load(pointsFile) && loaded.points.size() == 1);
    CHECK(write(pointsFile, R"({"points":[{"name":"P1","type":"joint","joint":[0,0,0,0,0,0],"tool":"TOOL0"}]})"));
    CHECK(!loaded.load(pointsFile) && loaded.points.size() == 1);
    const QString flowFile = dir.filePath("flow.json");
    FlowStore flows; Flow flow; flow.name = "test";
    FlowBlock move; move.type = FlowBlock::MOVEJ; move.params["point"] = "P1"; move.params["speed"] = 35; flow.blocks.append(move);
    FlowBlock linear; linear.type = FlowBlock::MOVEL; linear.params["point"] = "P2"; linear.params["speed"] = 60; flow.blocks.append(linear);
    FlowBlock block; block.type = FlowBlock::IO_WRITE; block.params["index"] = 3; block.params["value"] = 1; flow.blocks.append(block);
    CHECK(flows.save(flowFile, flow));
    Flow loadedFlow; CHECK(flows.load(flowFile, loadedFlow) && loadedFlow.name == "test" && loadedFlow.blocks.size() == 3 && loadedFlow.blocks[0].params.value("speed").toInt() == 35 && loadedFlow.blocks[1].params.value("speed").toInt() == 60 && loadedFlow.blocks[2].params.value("index").toInt() == 3);
    CHECK(write(flowFile, "{}"));
    CHECK(!flows.load(flowFile, loadedFlow) && loadedFlow.name == "test");
    CHECK(write(flowFile, R"({"blocks":[{"type":3,"params":{"percent":20}},{"type":4,"params":{"index":3,"value":1}}]})"));
    CHECK(flows.load(flowFile, loadedFlow) && loadedFlow.blocks[0].type == FlowBlock::SET_SPEED && loadedFlow.blocks[1].type == FlowBlock::IO_WRITE);
    CHECK(write(flowFile, R"({"blocks":[{"type":4,"params":{"index":-1,"value":1}}]})"));
    CHECK(!flows.load(flowFile, loadedFlow));
}
