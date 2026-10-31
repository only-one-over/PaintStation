#pragma once

#include <QString>
#include <QVector>
#include "RobotTypes.h"

// 点位存储：userdata/points/points.json
class PointStore
{
public:
    bool load(const QString& file);
    bool save(const QString& file) const;

    QVector<RobotPoint> points;
};
