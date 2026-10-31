#pragma once

#include <QString>
#include <QVector>
#include "RobotTypes.h"

// 流程存储：userdata/flows/<name>.json
class FlowStore
{
public:
    bool load(const QString& file, Flow& out);
    bool save(const QString& file, const Flow& flow);
    QStringList listFlows(const QString& dir);
};
