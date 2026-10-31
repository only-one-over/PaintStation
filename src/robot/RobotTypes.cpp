#include "RobotTypes.h"
#include <QJsonArray>

QVariantMap RobotPoint::toJson() const
{
    QVariantMap m;
    m["name"] = name;
    m["type"] = (type == JOINT) ? "joint" : "cartesian";
    QJsonArray jarr;
    for (int i = 0; i < 6; ++i) jarr.append(j[i]);
    m["joint"] = jarr;
    m["x"]=x; m["y"]=y; m["z"]=z; m["a"]=a; m["b"]=b; m["c"]=c;
    m["tool"] = tool;
    m["comment"] = comment;
    return m;
}

void RobotPoint::fromJson(const QVariantMap& m)
{
    name = m.value("name").toString();
    type = (m.value("type").toString() == "joint") ? JOINT : CARTESIAN;
    QJsonArray jarr = m.value("joint").toJsonArray();
    for (int i = 0; i < 6 && i < jarr.size(); ++i) j[i] = jarr[i].toDouble();
    x=m.value("x").toDouble(); y=m.value("y").toDouble(); z=m.value("z").toDouble();
    a=m.value("a").toDouble(); b=m.value("b").toDouble(); c=m.value("c").toDouble();
    tool = m.value("tool").toString();
    if (tool.isEmpty()) tool = "TOOL0";
    comment = m.value("comment").toString();
}

QString FlowBlock::typeName(Type t)
{
    switch (t) {
        case MOVEJ:     return "关节运动";
        case MOVEL:     return "直线运动";
        case DELAY:     return "延时";
        case SET_SPEED: return "设置速度";
        case IO_WRITE:  return "IO 输出";
        default:        return "未知";
    }
}

QString FlowBlock::typeHint(Type t)
{
    switch (t) {
        case MOVEJ:     return "关节 movej 到指定点位";
        case MOVEL:     return "直线 movel 到指定点位";
        case DELAY:     return "等待 N 秒";
        case SET_SPEED: return "设置全局速度百分比";
        case IO_WRITE:  return "写数字输出 DO[index] = value";
        default:        return "";
    }
}

QVariantMap FlowBlock::toJson() const
{
    QVariantMap m;
    m["type"] = static_cast<int>(type);
    m["params"] = params;
    return m;
}

void FlowBlock::fromJson(const QVariantMap& m)
{
    type = static_cast<Type>(m.value("type", MOVEJ).toInt());
    params = m.value("params").toMap();
}

QVariantMap Flow::toJson() const
{
    QVariantMap m;
    m["name"] = name;
    QVariantList list;
    for (const auto& b : blocks) list.append(b.toJson());
    m["blocks"] = list;
    return m;
}

void Flow::fromJson(const QVariantMap& m)
{
    name = m.value("name").toString();
    blocks.clear();
    for (const auto& v : m.value("blocks").toList()) {
        FlowBlock b;
        b.fromJson(v.toMap());
        blocks.append(b);
    }
}
