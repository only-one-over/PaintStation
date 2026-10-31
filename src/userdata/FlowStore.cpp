#include "FlowStore.h"
#include <QFile>
#include <QSaveFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <cmath>
#include <QDir>

bool FlowStore::load(const QString& file, Flow& out)
{
    QFile f(file);
    if (!f.open(QIODevice::ReadOnly)) return false;
    const auto doc = QJsonDocument::fromJson(f.readAll());
    if (!doc.isObject()) return false;
    auto root = doc.object();
    if (!root.value("blocks").isArray()) return false;
    for (const auto& value : root.value("blocks").toArray()) {
        if (!value.isObject()) return false;
        const auto block = value.toObject();
        const auto typeValue = block.value("type");
        if (!typeValue.isDouble() || typeValue.toDouble() != typeValue.toInt(-1) || !block.value("params").isObject()) return false;
        const int type = typeValue.toInt(-1);
        const auto params = block.value("params").toObject();
        auto number = [&](const char* key, double low, double high) {
            const auto v = params.value(key);
            return v.isDouble() && std::isfinite(v.toDouble()) && v.toDouble() >= low && v.toDouble() <= high;
        };
        if (type == FlowBlock::MOVEJ || type == FlowBlock::MOVEL) {
            if (!params.value("point").isString() || params.value("point").toString().trimmed().isEmpty() || !number("speed", 1, 100)) return false;
        } else if (type == FlowBlock::DELAY) {
            if (!number("sec", 0, 999)) return false;
        } else if (type == FlowBlock::SET_SPEED) {
            if (!number("percent", 1, 100)) return false;
        } else if (type == FlowBlock::IO_WRITE) {
            if (!number("index", 0, 255) || params.value("index").toDouble() != params.value("index").toInt() ||
                !number("value", 0, 1) || params.value("value").toDouble() != params.value("value").toInt()) return false;
        } else return false;
    }
    out.fromJson(root.toVariantMap());
    return true;
}

bool FlowStore::save(const QString& file, const Flow& flow)
{
    QSaveFile f(file);
    if (!f.open(QIODevice::WriteOnly)) return false;
    const auto data = QJsonDocument(QJsonObject::fromVariantMap(flow.toJson())).toJson(QJsonDocument::Indented);
    return f.write(data) == data.size() && f.commit();
}

QStringList FlowStore::listFlows(const QString& dir)
{
    return QDir(dir).entryList(QStringList() << "*.json", QDir::Files, QDir::Name);
}
