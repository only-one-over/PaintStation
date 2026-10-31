#pragma once

#include <QString>
#include <QVector>
#include <QVariantMap>

// 一个机器人点位
struct RobotPoint {
    QString name;                       // 唯一名称，如 "P1_起喷"
    enum Type { JOINT, CARTESIAN } type = JOINT;

    // 关节坐标（度）
    double j[6] = {0,0,0,0,0,0};

    // 笛卡尔坐标（mm, deg）
    double x=0, y=0, z=0, a=0, b=0, c=0;

    QString tool = "TOOL0";
    QString comment;

    // JSON 序列化
    QVariantMap toJson() const;
    void fromJson(const QVariantMap& m);
};

// 流程里的一个步骤块
struct FlowBlock {
    // 模块类型
    enum Type {
        MOVEJ,      // 关节运动到点位
        MOVEL,      // 直线运动到点位
        DELAY,      // 延时
        SET_SPEED,  // 保留旧文件的类型编号，不再提供此模块
        IO_WRITE,   // 写 DO（索引+值）
        TYPE_COUNT
    };
    Type type = MOVEJ;
    QVariantMap params;   // 不同类型参数不同

    static QString typeName(Type t);
    static QString typeHint(Type t);   // 模块描述
    QVariantMap toJson() const;
    void fromJson(const QVariantMap& m);
};

// 一条完整流程
struct Flow {
    QString name;
    QVector<FlowBlock> blocks;

    QVariantMap toJson() const;
    void fromJson(const QVariantMap& m);
};
