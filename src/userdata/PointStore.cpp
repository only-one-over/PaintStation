#include "PointStore.h"
#include <QFile>
#include <QSaveFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QSet>
#include <cmath>

bool PointStore::load(const QString& file)
{
    QFile f(file);
    if (!f.open(QIODevice::ReadOnly)) return false;
    const auto doc = QJsonDocument::fromJson(f.readAll());
    if (!doc.isObject()) return false;
    auto root = doc.object();
    if (!root.value("points").isArray()) return false;
    QVector<RobotPoint> loaded;
    QSet<QString> names;
    for (const auto& v : root.value("points").toArray()) {
        if (!v.isObject()) return false;
        const auto obj = v.toObject();
        const auto name = obj.value("name").toString().trimmed();
        const auto type = obj.value("type").toString();
        if (name.isEmpty() || names.contains(name) || (type != "joint" && type != "cartesian") ||
            !obj.value("tool").isString() || obj.value("tool").toString().trimmed().isEmpty()) return false;
        const auto joints = obj.value("joint").toArray();
        if (joints.size() != 6) return false;
        for (const auto& joint : joints) if (!joint.isDouble() || !std::isfinite(joint.toDouble())) return false;
        for (const char* key : {"x", "y", "z", "a", "b", "c"})
            if (!obj.value(key).isDouble() || !std::isfinite(obj.value(key).toDouble())) return false;
        RobotPoint p;
        p.fromJson(obj.toVariantMap());
        for (int i = 0; i < 6; ++i) p.j[i] = joints[i].toDouble();
        loaded.append(p);
        names.insert(name);
    }
    points = loaded;
    return true;
}

bool PointStore::save(const QString& file) const
{
    QJsonArray arr;
    for (const auto& p : points) arr.append(QJsonObject::fromVariantMap(p.toJson()));
    QJsonObject root;
    root["points"] = arr;
    QSaveFile f(file);
    if (!f.open(QIODevice::WriteOnly)) return false;
    const auto data = QJsonDocument(root).toJson(QJsonDocument::Indented);
    return f.write(data) == data.size() && f.commit();
}
