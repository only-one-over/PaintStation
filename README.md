# PaintStation

喷涂手眼标定 & 视觉定位上位机（Qt6 + C++17）。

## 目录结构（四层架构）

```
PaintStation/
├── CMakeLists.txt
├── config/
│   └── app_config.json        # 首次运行自动生成
├── data/                       # 手眼矩阵 / 相机内参保存目录
└── src/
    ├── main.cpp
    ├── ui/                     # UI 层：界面，不直接碰 SDK/硬件
    │   ├── MainWindow.{h,cpp}
    │   ├── ConfigPage.{h,cpp}        # 第 1 页：机器人/相机/标定板配置
    │   ├── CalibVerifyPage.{h,cpp}   # 第 2 页：实时图像 + 点像素出 3D 坐标
    │   └── ImageWidget.{h,cpp}       # 可点击显示 OpenCV 图像
    ├── robot/                  # 机器人层：封装埃夫特 EfortSdk
    │   └── RobotController.{h,cpp}
    ├── vision/                 # 视觉层：相机 + 手眼标定
    │   ├── Camera.h              # 抽象接口
    │   ├── RealSenseCamera.{h,cpp}  # D435i 实现（深度对齐彩色）
    │   └── HandEyeCalibrator.{h,cpp} # eye-to-hand 标定
    └── userdata/               # 配置数据层：JSON 读写
        └── AppConfig.{h,cpp}
```

## 依赖

| 组件 | 版本 | 说明 |
|------|------|------|
| Qt6 | ≥ 6.2 | Widgets |
| OpenCV | ≥ 4.x | 棋盘格检测 / solvePnP / calibrateHandEye |
| librealsense2 | ≥ 2.50 | D435i 取流 |
| 埃夫特 SDK | V2.8.0 | 已在 `../V2.8.0/`，CMake 自动链接 |

## 编译

```powershell
cd D:\桌面\喷涂\PaintStation
cmake -B build -S . -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

运行时 exe 旁会自动拷入 EftSdk.dll 等；librealsense2.dll 和 opencv_worldxxx.dll 需自行放入 PATH 或 exe 目录。

## 使用流程

### 第 1 页 配置
1. 填机器人 IP（默认 `192.168.1.6`），点"连接机器人"
2. 填 D435i 序列号（留空=第一台），分辨率 1280x720@30，点"打开相机"
3. 填棋盘格内角点数（如 11x8）和单格边长（mm）
4. 点"保存配置"

### 标定数据采集与验证（eye-to-hand，相机固定）
1. 棋盘格固定在机器人末端，移动到不同姿态并逐组采集图像与机器人位姿。采集时应保持机器人静止，图像与位姿不是硬件同步的。
2. 导出 JSON 和图像，在外部软件检测棋盘格并求解手眼矩阵；本程序不做角点检测或矩阵求解。
3. 导入外部 `T_cam2_base` JSON 后，点击有效深度像素查看基坐标。矩阵格式为 `{"hasMatrix":true,"T_cam2_base":{"r0c0":1,...,"r3c3":1}}`，需提供全部 16 个数值；平移单位为毫米。
4. 点位管理与流程编辑可创建和运行运动、延时与 IO 步骤。全局速度步骤尚未实现，执行到该步骤会报错。

暂停只阻止进入后续步骤，不会暂停当前运动；停止使用 SDK 的 MOVEHOLD，不等同于安全急停。运动指令返回是否代表到位需要按控制器和 SDK 验证。实际运行前应验证坐标、速度、运动范围和独立安全回路。

## 坐标约定
- 机器人位姿：x,y,z (mm)；a,b,c 为 ZYX 欧拉角（度）
- 手眼矩阵 T_cam2_base：4x4 齐次变换，相机坐标系 -> 机器人基坐标系
- 像素到 3D：D435i 深度对齐彩色后，用内参反投影得相机系点，再左乘 T_cam2_base

## 已知 TODO
- [ ] 标定前相机内参目前用 RealSense 出厂内参，如需高精度请单独做内参标定
- [ ] 外部求解所用的机器人 a,b,c 欧拉角顺序和工具坐标系需现场核对
- [ ] 急停/清报警按钮还没放到 UI 上
- [ ] 外部求解的标定残差和验证结果需单独检查
