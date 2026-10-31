#include "FlowEditPage.h"
#include "RobotController.h"

#include <QListWidget>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QStackedWidget>
#include <QGroupBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QGridLayout>
#include <QFileDialog>
#include <QDir>
#include <QFileInfo>
#include <QMessageBox>
#include <QThread>
#include <QDebug>
#include <QSet>
#include <cmath>

static QString validateFlow(const Flow& flow, const PointStore& points)
{
    QSet<QString> names;
    for (const auto& point : points.points) {
        if (point.name.trimmed().isEmpty() || point.tool.trimmed().isEmpty() || names.contains(point.name)) return "点位名称重复或工具名为空";
        names.insert(point.name);
        for (double j : point.j) if (!std::isfinite(j)) return "关节点位无效";
        for (double x : {point.x, point.y, point.z, point.a, point.b, point.c}) if (!std::isfinite(x)) return "笛卡尔点位无效";
    }
    for (int i = 0; i < flow.blocks.size(); ++i) {
        const auto& b = flow.blocks[i];
        bool valid = true;
        if (b.type == FlowBlock::MOVEJ || b.type == FlowBlock::MOVEL) {
            bool speedOk = false;
            double speed = b.params.value("speed").toDouble(&speedOk);
            valid = speedOk && std::isfinite(speed) && speed >= 1 && speed <= 100 && std::floor(speed) == speed;
            bool found = false;
            for (const auto& p : points.points) if (p.name == b.params.value("point").toString() &&
                ((b.type == FlowBlock::MOVEJ && p.type == RobotPoint::JOINT) || (b.type == FlowBlock::MOVEL && p.type == RobotPoint::CARTESIAN))) found = true;
            valid = valid && found;
        } else if (b.type == FlowBlock::DELAY) {
            bool ok = false; const double sec = b.params.value("sec").toDouble(&ok);
            valid = ok && std::isfinite(sec) && sec >= 0 && sec <= 999;
        } else if (b.type == FlowBlock::IO_WRITE) {
            bool ok = false; const int index = b.params.value("index").toInt(&ok);
            bool valueOk = false; const int value = b.params.value("value").toInt(&valueOk);
            valid = ok && index >= 0 && index <= 255 && valueOk && (value == 0 || value == 1);
        } else if (b.type == FlowBlock::SET_SPEED) {
            return QString("步骤 %1 是旧版速度模块，请删除并在各运动步骤中设置速度").arg(i + 1);
        } else valid = false;
        if (!valid) return QString("步骤 %1 无效，请检查点位及整数速度").arg(i + 1);
    }
    return {};
}

// ============== FlowWorker ==============
FlowWorker::FlowWorker(RobotController* robot, PointStore* points)
    : m_robot(robot), m_snapshot(*points) {}

void FlowWorker::runFlow(Flow flow, int startIndex)
{
    if (!m_robot || !m_robot->isConnected()) {
        emit finished(false, "机器人未连接");
        return;
    }

    for (int i = startIndex; i < flow.blocks.size(); ++i) {
        if (m_stop) { emit finished(false, "已停止"); return; }

        while (m_paused && !m_stop) {
            QThread::msleep(50);
        }
        if (m_stop) { emit finished(false, "已停止"); return; }

        const auto& b = flow.blocks[i];
        emit blockStarted(i);
        qInfo() << "[Flow] step" << i << FlowBlock::typeName(b.type);

        bool ok = true;
        switch (b.type) {
            case FlowBlock::MOVEJ:
            case FlowBlock::MOVEL: {
                QString pname = b.params.value("point").toString();
                double spd = b.params.value("speed", 20).toDouble();
                RobotPoint* target = nullptr;
                for (auto& p : m_snapshot.points) if (p.name == pname) { target = &p; break; }
                if (!target || (b.type == FlowBlock::MOVEJ && target->type != RobotPoint::JOINT) ||
                    (b.type == FlowBlock::MOVEL && target->type != RobotPoint::CARTESIAN)) { ok = false; break; }
                if (b.type == FlowBlock::MOVEJ)
                    ok = m_robot->moveJoint(target->j, spd, target->tool);
                else {
                    ToolPose tp{target->x, target->y, target->z,
                                target->a, target->b, target->c};
                    ok = m_robot->moveCartesian(tp, spd, target->tool);
                }
                break;
            }
            case FlowBlock::DELAY: {
                double sec = b.params.value("sec", 1.0).toDouble();
                int waited = 0;
                while (waited < static_cast<int>(sec*1000)) {
                    if (m_stop) { emit finished(false,"已停止"); return; }
                    QThread::msleep(50);
                    waited += 50;
                }
                break;
            }
            case FlowBlock::SET_SPEED:
                emit finished(false, "设置全局速度尚未实现"); return;
            case FlowBlock::IO_WRITE: {
                int idx = b.params.value("index", 0).toInt();
                int val = b.params.value("value", 0).toInt();
                ok = m_robot->writeDO(idx, val != 0);
                break;
            }
            default: ok = false; break;
        }
        if (!ok) { emit finished(false, QString("步骤 %1 执行失败").arg(i+1)); return; }
    }
    emit finished(true, "流程执行完成");
}

void FlowWorker::runOneStep(Flow flow, int index)
{
    if (!m_robot || !m_robot->isConnected()) {
        emit finished(false, "机器人未连接"); return;
    }
    if (index < 0 || index >= flow.blocks.size()) {
        emit finished(false, "步骤索引无效"); return;
    }
    if (m_stop) { emit finished(false, "已停止"); return; }
    const auto& b = flow.blocks[index];
    emit blockStarted(index);
    bool ok = true;
    switch (b.type) {
        case FlowBlock::MOVEJ:
        case FlowBlock::MOVEL: {
            QString pname = b.params.value("point").toString();
            double spd = b.params.value("speed", 20).toDouble();
            RobotPoint* target = nullptr;
            for (auto& p : m_snapshot.points) if (p.name == pname) { target = &p; break; }
            if (!target || (b.type == FlowBlock::MOVEJ && target->type != RobotPoint::JOINT) ||
                (b.type == FlowBlock::MOVEL && target->type != RobotPoint::CARTESIAN)) ok = false;
            else if (b.type == FlowBlock::MOVEJ) ok = m_robot->moveJoint(target->j, spd, target->tool);
            else {
                ToolPose tp{target->x, target->y, target->z, target->a, target->b, target->c};
                ok = m_robot->moveCartesian(tp, spd, target->tool);
            }
            break;
        }
        case FlowBlock::DELAY: {
            double sec = b.params.value("sec", 1.0).toDouble();
            int waited = 0;
            while (waited < static_cast<int>(sec * 1000)) {
                if (m_stop) { emit finished(false, "已停止"); return; }
                QThread::msleep(10);
                waited += 10;
            }
            break;
        }
        case FlowBlock::IO_WRITE:
            ok = m_robot->writeDO(b.params.value("index",0).toInt(),
                                  b.params.value("value",0).toInt() != 0);
            break;
        default: ok = false; break;
    }
    emit finished(ok, ok ? QString("步骤 %1 完成").arg(index+1) : QString("步骤 %1 失败").arg(index+1));
}

// ============== FlowEditPage ==============
FlowEditPage::FlowEditPage(RobotController* robot, PointStore* points, QWidget* parent)
    : QWidget(parent), m_robot(robot), m_points(points)
{
    m_worker = new FlowWorker(robot, points);
    m_worker->moveToThread(&m_workerThread);
    connect(&m_workerThread, &QThread::finished, m_worker, &QObject::deleteLater);
    connect(this, &FlowEditPage::destroyed, &m_workerThread, &QThread::quit);
    connect(m_worker, &FlowWorker::blockStarted, this, [this](int idx){
        m_blockList->setCurrentRow(idx);
        if (auto* item = m_blockList->item(idx)) item->setForeground(QColor("#22c55e"));
    });
    connect(m_worker, &FlowWorker::finished, this, &FlowEditPage::onWorkerFinished);
    m_workerThread.start();

    buildUi();
    refreshPointCombo();
}

FlowEditPage::~FlowEditPage()
{
    if (m_running) m_robot->stopMotion();
    m_worker->requestStop();
    m_workerThread.quit();
    m_workerThread.wait();
    if (m_running) m_robot->endFlow();
}

void FlowEditPage::buildUi()
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(16,16,16,16); root->setSpacing(12);

    // 顶部
    auto* top = new QHBoxLayout;
    auto* title = new QLabel("流程编辑"); title->setObjectName("titleLabel");
    top->addWidget(title);
    top->addSpacing(20);
    top->addWidget(new QLabel("流程名:"));
    m_flowName = new QLineEdit("流程1");
    top->addWidget(m_flowName, 1);
    auto* btnNew = new QPushButton("新建"); btnNew->setProperty("secondary", true);
    auto* btnSave= new QPushButton("保存");
    auto* btnLoad= new QPushButton("加载"); btnLoad->setProperty("secondary", true);
    top->addWidget(btnNew); top->addWidget(btnSave); top->addWidget(btnLoad);
    root->addLayout(top);

    // 三列
    auto* body = new QHBoxLayout; body->setSpacing(12);

    auto* libBox = new QGroupBox("模块库（点击添加）");
    auto* libLay = new QVBoxLayout(libBox);
    for (int t = 0; t < FlowBlock::TYPE_COUNT; ++t) {
        if (t == FlowBlock::SET_SPEED) continue;
        auto* b = new QPushButton(FlowBlock::typeName(static_cast<FlowBlock::Type>(t)));
        b->setToolTip(FlowBlock::typeHint(static_cast<FlowBlock::Type>(t)));
        connect(b, &QPushButton::clicked, this, [this,t]{ onAddBlock(t); });
        libLay->addWidget(b);
    }
    libLay->addStretch();
    body->addWidget(libBox, 1);

    auto* midBox = new QGroupBox("步骤序列");
    auto* midLay = new QVBoxLayout(midBox);
    m_blockList = new QListWidget;
    midLay->addWidget(m_blockList, 1);
    auto* moveRow = new QHBoxLayout;
    auto* btnUp = new QPushButton("上移"); btnUp->setProperty("secondary", true);
    auto* btnDown = new QPushButton("下移"); btnDown->setProperty("secondary", true);
    auto* btnDel = new QPushButton("删除"); btnDel->setProperty("secondary", true);
    moveRow->addWidget(btnUp); moveRow->addWidget(btnDown); moveRow->addWidget(btnDel);
    midLay->addLayout(moveRow);
    body->addWidget(midBox, 2);

    auto* paramBox = new QGroupBox("参数");
    auto* pLay = new QVBoxLayout(paramBox);
    m_paramStack = new QStackedWidget;

    auto* pMove = new QWidget; auto* fMove = new QFormLayout(pMove);
    m_pointCombo = new QComboBox;
    m_speed = new QSpinBox; m_speed->setRange(1,100); m_speed->setValue(20); m_speed->setSuffix("");
    fMove->addRow("点位", m_pointCombo);
    fMove->addRow("速度（关节 % / 直线 mm/s）", m_speed);
    m_paramStack->addWidget(pMove);

    auto* pDelay = new QWidget; auto* fDelay = new QFormLayout(pDelay);
    m_delaySec = new QDoubleSpinBox; m_delaySec->setRange(0,999); m_delaySec->setSuffix(" s");
    fDelay->addRow("延时", m_delaySec);
    m_paramStack->addWidget(pDelay);

    m_paramStack->addWidget(new QLabel("旧版速度模块已停用，请删除此步骤并在各运动步骤中设置速度。"));

    auto* pIO = new QWidget; auto* fIO = new QFormLayout(pIO);
    m_doIndex = new QSpinBox; m_doIndex->setRange(0,255);
    m_doValue = new QComboBox; m_doValue->addItem("OFF (0)", 0); m_doValue->addItem("ON (1)", 1);
    fIO->addRow("DO 索引", m_doIndex);
    fIO->addRow("值", m_doValue);
    m_paramStack->addWidget(pIO);

    pLay->addWidget(m_paramStack);
    auto* btnApply = new QPushButton("应用参数");
    pLay->addWidget(btnApply);
    body->addWidget(paramBox, 1);

    root->addLayout(body, 1);

    // 底部执行栏
    auto* execBar = new QHBoxLayout;
    execBar->addStretch();
    m_btnPlay = new QPushButton("▶ 执行");
    m_btnPlay->setMinimumWidth(100);
    m_btnPlay->setStyleSheet("font-weight:600;background:#22c55e;color:#0a0a0a;");
    m_btnStep = new QPushButton("⏭ 单步");
    m_btnStep->setMinimumWidth(80);
    m_btnStep->setProperty("secondary", true);
    m_btnPause = new QPushButton("暂停");
    m_btnPause->setMinimumWidth(80);
    m_btnPause->setEnabled(false);
    m_btnStop = new QPushButton("■ 停止");
    m_btnStop->setMinimumWidth(80);
    m_btnStop->setEnabled(false);
    execBar->addWidget(m_btnPlay);
    execBar->addWidget(m_btnStep);
    execBar->addWidget(m_btnPause);
    execBar->addWidget(m_btnStop);
    root->addLayout(execBar);

    // 信号
    connect(m_blockList, &QListWidget::currentRowChanged, this, &FlowEditPage::onBlockSelected);
    connect(btnUp, &QPushButton::clicked, this, &FlowEditPage::onMoveUp);
    connect(btnDown, &QPushButton::clicked, this, &FlowEditPage::onMoveDown);
    connect(btnDel, &QPushButton::clicked, this, &FlowEditPage::onDeleteBlock);
    connect(btnApply, &QPushButton::clicked, this, &FlowEditPage::onApplyParams);
    connect(btnNew, &QPushButton::clicked, this, &FlowEditPage::onNewFlow);
    connect(btnSave, &QPushButton::clicked, this, &FlowEditPage::onSaveFlow);
    connect(btnLoad, &QPushButton::clicked, this, &FlowEditPage::onLoadFlow);
    connect(m_btnPlay, &QPushButton::clicked, this, &FlowEditPage::onPlay);
    connect(m_btnStep, &QPushButton::clicked, this, &FlowEditPage::onStepOnce);
    connect(m_btnPause, &QPushButton::clicked, this, &FlowEditPage::onPauseResume);
    connect(m_btnStop, &QPushButton::clicked, this, &FlowEditPage::onStop);
}

void FlowEditPage::refreshPointCombo()
{
    const QString selected = m_pointCombo->currentText();
    m_pointCombo->clear();
    for (const auto& p : m_points->points) m_pointCombo->addItem(p.name);
    m_pointCombo->setCurrentText(selected);
}

void FlowEditPage::refreshBlockList()
{
    m_blockList->clear();
    for (int i = 0; i < m_flow.blocks.size(); ++i) {
        const auto& b = m_flow.blocks[i];
        QString text = QString("%1. %2").arg(i+1).arg(FlowBlock::typeName(b.type));
        if (b.type == FlowBlock::MOVEJ || b.type == FlowBlock::MOVEL)
            text += QString(" -> %1 @ %2 %3").arg(b.params.value("point").toString()).arg(b.params.value("speed",20).toDouble()).arg(b.type == FlowBlock::MOVEJ ? "%" : "mm/s");
        else if (b.type == FlowBlock::DELAY)
            text += QString(" %1s").arg(b.params.value("sec",1.0).toDouble());
        else if (b.type == FlowBlock::IO_WRITE)
            text += QString(" DO[%1]=%2").arg(b.params.value("index",0).toInt()).arg(b.params.value("value",0).toInt());
        m_blockList->addItem(text);
    }
}

void FlowEditPage::onAddBlock(int type)
{
    if (m_running) return;
    FlowBlock b;
    b.type = static_cast<FlowBlock::Type>(type);
    if (type == FlowBlock::MOVEJ || type == FlowBlock::MOVEL) {
        b.params["point"] = m_pointCombo->currentText();
        b.params["speed"]  = 20;
    } else if (type == FlowBlock::DELAY) {
        b.params["sec"] = 1.0;
    } else if (type == FlowBlock::IO_WRITE) {
        b.params["index"] = 0;
        b.params["value"] = 0;
    }
    m_flow.blocks.append(b);
    refreshBlockList();
    m_blockList->setCurrentRow(m_flow.blocks.size()-1);
}

void FlowEditPage::onBlockSelected(int row)
{
    m_currentRow = row;
    if (row < 0 || row >= m_flow.blocks.size()) return;
    const auto& b = m_flow.blocks[row];
    switch (b.type) {
        case FlowBlock::MOVEJ:
        case FlowBlock::MOVEL:
            m_paramStack->setCurrentIndex(0);
            m_pointCombo->setCurrentText(b.params.value("point").toString());
            m_speed->setValue(b.params.value("speed", 20).toInt());
            break;
        case FlowBlock::DELAY:
            m_paramStack->setCurrentIndex(1);
            m_delaySec->setValue(b.params.value("sec", 1.0).toDouble());
            break;
        case FlowBlock::SET_SPEED:
            m_paramStack->setCurrentIndex(2);
            break;
        case FlowBlock::IO_WRITE:
            m_paramStack->setCurrentIndex(3);
            m_doIndex->setValue(b.params.value("index", 0).toInt());
            m_doValue->setCurrentIndex(b.params.value("value", 0).toInt());
            break;
        default: break;
    }
}

void FlowEditPage::onApplyParams()
{
    if (m_running) return;
    if (m_currentRow < 0) return;
    auto& b = m_flow.blocks[m_currentRow];
    switch (b.type) {
        case FlowBlock::MOVEJ:
        case FlowBlock::MOVEL:
            b.params["point"] = m_pointCombo->currentText();
            b.params["speed"]  = m_speed->value();
            break;
        case FlowBlock::DELAY:
            b.params["sec"] = m_delaySec->value();
            break;
        case FlowBlock::SET_SPEED:
            return;
        case FlowBlock::IO_WRITE:
            b.params["index"] = m_doIndex->value();
            b.params["value"] = m_doValue->currentData().toInt();
            break;
    }
    refreshBlockList();
}

void FlowEditPage::onDeleteBlock()
{
    if (m_running) return;
    if (m_currentRow < 0) return;
    m_flow.blocks.removeAt(m_currentRow);
    m_currentRow = -1;
    refreshBlockList();
}

void FlowEditPage::onMoveUp()
{
    if (m_running) return;
    if (m_currentRow <= 0) return;
    m_flow.blocks.swapItemsAt(m_currentRow, m_currentRow-1);
    refreshBlockList();
    m_blockList->setCurrentRow(m_currentRow-1);
}

void FlowEditPage::onMoveDown()
{
    if (m_running) return;
    if (m_currentRow < 0 || m_currentRow >= m_flow.blocks.size()-1) return;
    m_flow.blocks.swapItemsAt(m_currentRow, m_currentRow+1);
    refreshBlockList();
    m_blockList->setCurrentRow(m_currentRow+1);
}

void FlowEditPage::onNewFlow()
{
    if (m_running) return;
    m_flow = Flow();
    m_flowName->setText("流程1");
    refreshBlockList();
}

void FlowEditPage::onSaveFlow()
{
    m_flow.name = m_flowName->text();
    if (m_flow.name.isEmpty()) { emit statusMessage("流程名不能为空"); return; }
    QString dir = QDir::current().filePath("userdata/flows");
    QDir().mkpath(dir);
    QString path = QDir(dir).filePath(m_flow.name + ".json");
    emit statusMessage(m_store.save(path, m_flow) ? "流程已保存: " + path : "流程保存失败: " + path);
}

void FlowEditPage::onLoadFlow()
{
    if (m_running) return;
    QString dir = QDir::current().filePath("userdata/flows");
    QString path = QFileDialog::getOpenFileName(this, "加载流程", dir, "JSON (*.json)");
    if (path.isEmpty()) return;
    if (m_store.load(path, m_flow)) {
        m_flowName->setText(m_flow.name);
        refreshBlockList();
        emit statusMessage("已加载: " + path);
    } else emit statusMessage("加载失败");
}

void FlowEditPage::onPlay()
{
    if (m_running) return;
    if (m_flow.blocks.isEmpty()) { emit statusMessage("流程为空"); return; }
    if (!m_robot->isConnected()) { emit statusMessage("机器人未连接"); return; }
    int start = m_currentRow;
    if (start < 0) start = 0;
    const QString validation = validateFlow(m_flow, *m_points);
    if (!validation.isEmpty()) { emit statusMessage(validation); return; }
    if (!m_robot->beginFlow()) { emit statusMessage("机器人忙或未连接"); return; }
    m_worker->prepare(*m_points);
    m_running = true;
    m_btnPlay->setEnabled(false);
    m_btnStep->setEnabled(false);
    m_btnPause->setEnabled(true);
    m_btnStop->setEnabled(true);
    m_btnPause->setText("暂停");
    for (int i = 0; i < m_blockList->count(); ++i)
        m_blockList->item(i)->setForeground(QColor("#e5e7eb"));
    QMetaObject::invokeMethod(m_worker, "runFlow", Qt::QueuedConnection,
                              Q_ARG(Flow, m_flow), Q_ARG(int, start));
    emit statusMessage(QString("从步骤 %1 开始执行").arg(start+1));
}

void FlowEditPage::onStepOnce()
{
    if (m_running) return;
    if (!m_robot->isConnected()) { emit statusMessage("机器人未连接"); return; }
    int idx = m_currentRow;
    if (idx < 0 || idx >= m_flow.blocks.size()) {
        emit statusMessage("请先在步骤列表选中要执行的步骤"); return;
    }
    const QString validation = validateFlow(m_flow, *m_points);
    if (!validation.isEmpty()) { emit statusMessage(validation); return; }
    if (!m_robot->beginFlow()) { emit statusMessage("机器人忙或未连接"); return; }
    m_worker->prepare(*m_points);
    m_running = true;
    m_btnPlay->setEnabled(false);
    m_btnStep->setEnabled(false);
    m_btnPause->setEnabled(false);
    m_btnStop->setEnabled(true);
    QMetaObject::invokeMethod(m_worker, "runOneStep", Qt::QueuedConnection,
                              Q_ARG(Flow, m_flow), Q_ARG(int, idx));
}

void FlowEditPage::onPauseResume()
{
    if (!m_running) return;
    m_isPaused = !m_isPaused;
    if (m_isPaused) {
        m_worker->requestPause();
        m_btnPause->setText("继续");
        emit statusMessage("已暂停");
    } else {
        m_worker->requestResume();
        m_btnPause->setText("暂停");
        emit statusMessage("继续执行");
    }
}

void FlowEditPage::stopFromPanel()
{
    if (m_running) { onStop(); return; }
    emit statusMessage(m_robot->stopMotion() ? "运动停止指令已发送" : "停止指令失败，请检查机器人");
}

void FlowEditPage::onStop()
{
    if (!m_running) return;
    const bool stopped = m_robot && m_robot->stopMotion();
    m_worker->requestStop();
    emit statusMessage(stopped ? "停止中..." : "停止指令失败，请检查机器人");
}

void FlowEditPage::onWorkerFinished(bool ok, const QString& msg)
{
    m_robot->endFlow();
    m_running = false;
    m_isPaused = false;
    m_btnPlay->setEnabled(true);
    m_btnStep->setEnabled(true);
    m_btnPause->setEnabled(false);
    m_btnStop->setEnabled(false);
    m_btnPause->setText("暂停");
    // 单步完成后自动选中下一步，方便连续按单步
    if (ok && m_currentRow >= 0 && m_currentRow + 1 < m_flow.blocks.size()) {
        m_blockList->setCurrentRow(m_currentRow + 1);
    }
    emit statusMessage(msg);
}
