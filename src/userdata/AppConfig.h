#pragma once

#include <QString>

struct RobotConfig {
    QString ip       = "192.168.1.6";
    int     port     = 5000;
    int     speed    = 20;
    QString toolName = "TOOL0";
};

struct CameraConfig {
    QString serialNumber;
    int     width  = 1280;
    int     height = 720;
    int     fps    = 30;
};

// 目录结构（相对 exe 工作目录，全部归在 userdata/ 下）：
//   userdata/
//     ├── config/
//     │   └── app_config.json
//     ├── calibration/
//     │   ├── sessions/YYYYMMDD_HHMMSS/
//     │   │   ├── handeye_dataset.json
//     │   │   └── images/
//     │   └── T_cam2_base.json
//     └── logs/
//         └── YYYY-MM-DD.log
struct PathConfig {
    QString rootDir            = "userdata";
    QString configDir          = "userdata/config";
    QString appConfigFile      = "userdata/config/app_config.json";

    QString calibDir           = "userdata/calibration";
    QString calibSessionsDir   = "userdata/calibration/sessions";
    QString handEyeMatrixFile  = "userdata/calibration/T_cam2_base.json";

    QString pointsDir          = "userdata/points";
    QString pointsFile         = "userdata/points/points.json";

    QString flowsDir           = "userdata/flows";

    QString logDir             = "userdata/logs";
};

struct AppConfig {
    RobotConfig      robot;
    CameraConfig     camera;
    PathConfig       paths;

    bool load(const QString& filePath);
    bool save(const QString& filePath) const;
    void ensureDirs() const;
};
