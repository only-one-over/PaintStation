#include "AppConfig.h"

#include <QFile>
#include <QSaveFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDir>
#include <QDebug>

bool AppConfig::load(const QString& filePath)
{
    QFile f(filePath);
    if (!f.open(QIODevice::ReadOnly)) {
        qWarning() << "[Config] not found, use defaults:" << filePath;
        return false;
    }
    const auto doc = QJsonDocument::fromJson(f.readAll());
    if (!doc.isObject()) return false;
    QJsonObject root = doc.object();
    f.close();

    if (root.contains("robot")) {
        auto o = root.value("robot").toObject();
        robot.ip       = o.value("ip").toString(robot.ip);
        robot.port     = o.value("port").toInt(robot.port);
        robot.speed    = o.value("speed").toInt(robot.speed);
        robot.toolName = o.value("toolName").toString(robot.toolName);
    }
    if (root.contains("camera")) {
        auto o = root.value("camera").toObject();
        camera.serialNumber = o.value("serialNumber").toString(camera.serialNumber);
        camera.width  = o.value("width").toInt(camera.width);
        camera.height = o.value("height").toInt(camera.height);
        camera.fps    = o.value("fps").toInt(camera.fps);
    }
    paths.handEyeMatrixFile = root.value("handEyeMatrixFile").toString(paths.handEyeMatrixFile);
    return true;
}

bool AppConfig::save(const QString& filePath) const
{
    QJsonObject root;
    QJsonObject rob;
    rob["ip"]=robot.ip; rob["port"]=robot.port; rob["speed"]=robot.speed; rob["toolName"]=robot.toolName;
    root["robot"]=rob;
    QJsonObject cam;
    cam["serialNumber"]=camera.serialNumber; cam["width"]=camera.width;
    cam["height"]=camera.height; cam["fps"]=camera.fps;
    root["camera"]=cam;
    root["handEyeMatrixFile"] = paths.handEyeMatrixFile;

    QSaveFile f(filePath);
    if (!f.open(QIODevice::WriteOnly)) return false;
    const auto data = QJsonDocument(root).toJson(QJsonDocument::Indented);
    return f.write(data) == data.size() && f.commit();
}

void AppConfig::ensureDirs() const
{
    for (const QString& d : {paths.rootDir, paths.configDir,
                              paths.calibDir, paths.calibSessionsDir,
                              paths.pointsDir, paths.flowsDir,
                              paths.logDir}) {
        QDir().mkpath(d);
    }
}
