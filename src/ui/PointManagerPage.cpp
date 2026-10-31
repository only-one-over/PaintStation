#include "PointManagerPage.h"
#include "RobotController.h"
#include "AppConfig.h"

#include <QTableWidget>
#include <QTableWidgetItem>
#include <QLineEdit>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QPushButton>
#include <QLabel>
#include <QGroupBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QHeaderView>
#include <QMessageBox>
#include <QDir>
#include <QFileInfo>
#include <QDateTime>
#include <QThread>
#include <memory>

PointManagerPage::PointManagerPage(RobotController* robot, PointStore* points, AppConfig* cfg, QWidget* parent)
    : QWidget(parent), m_robot(robot), m_store(points), m_cfg(cfg)
{
    buildUi();
    loadFromStore();
}

PointManagerPage::~PointManagerPage()
{
    if (m_moveThread) {
        m_robot->stopMotion();
        m_moveThread->wait();
        delete m_moveThread;
    }
}

void PointManagerPage::buildUi()
{
    auto* root = new QHBoxLayout(this);
    root->setContentsMargins(16,16,16,16); root->setSpacing(16);

    // ---- 左：点位表 ----
    auto* left = new QVBoxLayout;
    auto* title = new QLabel("点位列表"); title->setObjectName("titleLabel");
    left->addWidget(title);
    m_table = new QTableWidget(0, 3);
    m_table->setHorizontalHeaderLabels({"名称", "类型", "坐标"});
    m_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    left->addWidget(m_table, 1);
    root->addLayout(left, 1);

    // ---- 右：编辑表单 ----
    auto* right = new QVBoxLayout;
    right->setSpacing(10);

    auto* gb = new QGroupBox("点位编辑");
    auto* f = new QFormLayout(gb);
    f->setSpacing(8); f->setContentsMargins(16,20,16,16);

    m_name = new QLineEdit;
    f->addRow("名称", m_name);

    m_type = new QComboBox;
    m_type->addItem("关节", RobotPoint::JOINT);
    m_type->addItem("笛卡尔", RobotPoint::CARTESIAN);
    f->addRow("类型", m_type);

    // 关节
    auto* jbox = new QGroupBox("关节 (度)");
    auto* jlay = new QGridLayout(jbox);
    for (int i = 0; i < 6; ++i) {
        m_j[i] = new QDoubleSpinBox;
        m_j[i]->setRange(-360, 360); m_j[i]->setDecimals(2);
        jlay->addWidget(new QLabel(QString("J%1").arg(i+1)), i/3, (i%3)*2);
        jlay->addWidget(m_j[i], i/3, (i%3)*2+1);
    }
    f->addRow(jbox);

    // 笛卡尔
    auto* cbox = new QGroupBox("笛卡尔 (mm, deg)");
    auto* clay = new QGridLayout(cbox);
    const char* xyzabc[6] = {"X","Y","Z","A","B","C"};
    QDoubleSpinBox** arr[3] = {m_xyz, m_xyz+1, m_xyz+2};
    for (int i = 0; i < 6; ++i) {
        auto* sp = new QDoubleSpinBox;
        if (i < 3) { sp->setRange(-5000,5000); *arr[i] = sp; }
        else       { sp->setRange(-360,360); }
        sp->setDecimals(2);
        if (i < 3) m_xyz[i] = sp; else m_abc[i-3] = sp;
        clay->addWidget(new QLabel(xyzabc[i]), i/3, (i%3)*2);
        clay->addWidget(sp, i/3, (i%3)*2+1);
    }
    f->addRow(cbox);

    m_tool = new QLineEdit("TOOL0");
    f->addRow("工具", m_tool);
    m_comment = new QLineEdit;
    f->addRow("备注", m_comment);

    right->addWidget(gb);

    // 按钮
    m_btnReadCur = new QPushButton("读取当前位置");
    m_btnReadCur->setProperty("secondary", true);
    m_btnNew  = new QPushButton("新建点位");
    m_btnDel  = new QPushButton("删除选中");
    m_btnDel->setProperty("secondary", true);
    m_btnApply= new QPushButton("应用修改");

    auto* btnGrid = new QGridLayout;
    btnGrid->addWidget(m_btnReadCur, 0,0);
    btnGrid->addWidget(m_btnNew,     0,1);
    btnGrid->addWidget(m_btnDel,     1,0);
    btnGrid->addWidget(m_btnApply,   1,1);
    right->addLayout(btnGrid);
    m_btnMove = new QPushButton("运动到选中点位");
    right->addWidget(m_btnMove);
    connect(m_btnMove, &QPushButton::clicked, this, &PointManagerPage::onMoveToPoint);
    right->addStretch();

    root->addLayout(right, 1);

    connect(m_table, &QTableWidget::itemSelectionChanged, this, &PointManagerPage::onSelectionChanged);
    connect(m_btnNew, &QPushButton::clicked, this, &PointManagerPage::onNewPoint);
    connect(m_btnDel, &QPushButton::clicked, this, &PointManagerPage::onDeletePoint);
    connect(m_btnReadCur, &QPushButton::clicked, this, &PointManagerPage::onReadCurrent);
    connect(m_btnApply, &QPushButton::clicked, this, &PointManagerPage::onApplyEdit);
    connect(m_type, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int){
        bool isCart = m_type->currentData().toInt() == RobotPoint::CARTESIAN;
        for (int i = 0; i < 6; ++i) m_j[i]->setEnabled(!isCart);
        for (int i = 0; i < 3; ++i) m_xyz[i]->setEnabled(isCart);
        for (int i = 0; i < 3; ++i) m_abc[i]->setEnabled(isCart);
    });
}

void PointManagerPage::loadFromStore()
{
    QString path = QDir::current().filePath("userdata/points/points.json");
    QDir().mkpath(QFileInfo(path).absolutePath());
    if (QFileInfo::exists(path) && !m_store->load(path)) {
        QMessageBox::critical(this, "点位加载失败", "点位文件损坏或格式无效，请备份并修复后重启。禁止保存以保护原文件。");
        m_canSave = false;
    }
    refreshTable();
}

bool PointManagerPage::saveStore()
{
    QString path = QDir::current().filePath("userdata/points/points.json");
    return m_canSave && m_store->save(path);
}

void PointManagerPage::refreshTable()
{
    m_table->setRowCount(m_store->points.size());
    for (int i = 0; i < m_store->points.size(); ++i) {
        const auto& p = m_store->points[i];
        m_table->setItem(i, 0, new QTableWidgetItem(p.name));
        m_table->setItem(i, 1, new QTableWidgetItem(p.type == RobotPoint::JOINT ? "关节" : "笛卡尔"));
        QString coord;
        if (p.type == RobotPoint::JOINT)
            coord = QString("J1=%1 J2=%2").arg(p.j[0],0,'f',1).arg(p.j[1],0,'f',1);
        else
            coord = QString("X=%1 Y=%2 Z=%3").arg(p.x,0,'f',1).arg(p.y,0,'f',1).arg(p.z,0,'f',1);
        m_table->setItem(i, 2, new QTableWidgetItem(coord));
    }
}

void PointManagerPage::onSelectionChanged()
{
    auto rows = m_table->selectionModel()->selectedRows();
    if (rows.isEmpty()) { m_currentRow = -1; return; }
    m_currentRow = rows[0].row();
    const auto& p = m_store->points[m_currentRow];
    m_name->setText(p.name);
    m_type->setCurrentIndex(p.type == RobotPoint::JOINT ? 0 : 1);
    for (int i = 0; i < 6; ++i) m_j[i]->setValue(p.j[i]);
    m_xyz[0]->setValue(p.x); m_xyz[1]->setValue(p.y); m_xyz[2]->setValue(p.z);
    m_abc[0]->setValue(p.a); m_abc[1]->setValue(p.b); m_abc[2]->setValue(p.c);
    m_tool->setText(p.tool);
    m_comment->setText(p.comment);
}

void PointManagerPage::onNewPoint()
{
    RobotPoint p;
    p.name = QString("P%1").arg(m_store->points.size()+1);
    m_store->points.append(p);
    emit pointsChanged();
    refreshTable();
    m_table->selectRow(m_store->points.size()-1);
}

void PointManagerPage::onDeletePoint()
{
    if (m_currentRow < 0) return;
    m_store->points.removeAt(m_currentRow);
    emit pointsChanged();
    m_currentRow = -1;
    refreshTable();
    if (!saveStore()) emit statusMessage("点位保存失败");
}

void PointManagerPage::onReadCurrent()
{
    if (!m_robot->isConnected()) {
        emit statusMessage("机器人未连接"); return;
    }
    ToolPose tp;
    if (!m_robot->getCurrentToolPose(tp)) {
        emit statusMessage("读取当前位置失败"); return;
    }
    // 笛卡尔直接填
    m_type->setCurrentIndex(1);
    m_xyz[0]->setValue(tp.x); m_xyz[1]->setValue(tp.y); m_xyz[2]->setValue(tp.z);
    m_abc[0]->setValue(tp.a); m_abc[1]->setValue(tp.b); m_abc[2]->setValue(tp.c);
    emit statusMessage("已读取当前位置");
}

void PointManagerPage::onApplyEdit()
{
    if (m_currentRow < 0) return;
    const QString name = m_name->text().trimmed();
    if (name.isEmpty() || m_tool->text().trimmed().isEmpty()) { emit statusMessage("点位名称和工具名不能为空"); return; }
    for (int i = 0; i < m_store->points.size(); ++i)
        if (i != m_currentRow && m_store->points[i].name == name) { emit statusMessage("点位名称重复"); return; }
    auto& p = m_store->points[m_currentRow];
    p.name = name;
    p.type = m_type->currentData().toInt() == RobotPoint::CARTESIAN ? RobotPoint::CARTESIAN : RobotPoint::JOINT;
    for (int i = 0; i < 6; ++i) p.j[i] = m_j[i]->value();
    p.x = m_xyz[0]->value(); p.y = m_xyz[1]->value(); p.z = m_xyz[2]->value();
    p.a = m_abc[0]->value(); p.b = m_abc[1]->value(); p.c = m_abc[2]->value();
    p.tool = m_tool->text();
    p.comment = m_comment->text();
    emit pointsChanged();
    refreshTable();
    emit statusMessage(saveStore() ? "点位已保存" : "点位保存失败");
}

void PointManagerPage::onMoveToPoint()
{
    if (m_moveThread || m_currentRow < 0 || m_currentRow >= m_store->points.size()) {
        emit statusMessage("请先选择点位并等待当前运动结束"); return;
    }
    if (!m_canSave || !m_robot->isConnected()) { emit statusMessage("点位文件无效或机器人未连接"); return; }
    const RobotPoint point = m_store->points[m_currentRow];
    const int speed = m_cfg->robot.speed;
    QString coords;
    if (point.type == RobotPoint::JOINT) {
        for (double joint : point.j) coords += QString::number(joint, 'f', 2) + " ";
    } else {
        coords = QString("%1, %2, %3, %4, %5, %6").arg(point.x).arg(point.y).arg(point.z).arg(point.a).arg(point.b).arg(point.c);
    }
    const QString description = QString("点位：%1\n坐标：%2\n工具：%3\n速度：%4 %5\n\n确认运动到该点位？")
        .arg(point.name, coords, point.tool).arg(speed).arg(point.type == RobotPoint::JOINT ? "%" : "mm/s");
    if (QMessageBox::question(this, "确认机器人运动", description, QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes) return;
    if (!m_robot->beginFlow()) { emit statusMessage("机器人忙或未连接"); return; }
    auto result = std::make_shared<bool>(false);
    m_btnMove->setEnabled(false);
    m_moveThread = QThread::create([robot = m_robot, point, speed, result] {
        if (point.type == RobotPoint::JOINT) *result = robot->moveJoint(point.j, speed, point.tool);
        else {
            ToolPose pose{point.x, point.y, point.z, point.a, point.b, point.c};
            *result = robot->moveCartesian(pose, speed, point.tool);
        }
        robot->endFlow();
    });
    connect(m_moveThread, &QThread::finished, this, [this, result] {
        m_btnMove->setEnabled(true);
        m_moveThread->deleteLater();
        m_moveThread = nullptr;
        emit statusMessage(*result ? "机器人已到达点位" : "点位运动未完成，请检查机器人、工具和停止状态");
    });
    emit statusMessage("机器人正在运动到点位");
    m_moveThread->start();
}
