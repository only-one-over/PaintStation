#pragma once

#include <QWidget>
#include <QThread>
#include <atomic>
#include "FlowStore.h"
#include "PointStore.h"

class QListWidget;
class QComboBox;
class QDoubleSpinBox;
class QSpinBox;
class QLineEdit;
class QPushButton;
class QLabel;
class QStackedWidget;
class RobotController;

// 后台执行线程：顺序跑 blocks，支持暂停/继续/停止
class FlowWorker : public QObject
{
    Q_OBJECT
public:
    FlowWorker(RobotController* robot, PointStore* points);

public slots:
    void runFlow(Flow flow, int startIndex = 0);
    void runOneStep(Flow flow, int index);
    void prepare(const PointStore& points) { m_snapshot = points; m_stop = false; m_paused = false; }
    void requestPause()  { m_paused = true;  }
    void requestResume()  { m_paused = false; }
    void requestStop()    { m_stop = true;    }

signals:
    void blockStarted(int index);
    void finished(bool ok, const QString& msg);

private:
    RobotController* m_robot;
    PointStore       m_snapshot;
    std::atomic<bool> m_paused{false};
    std::atomic<bool> m_stop{false};
};

class FlowEditPage : public QWidget
{
    Q_OBJECT
public:
    FlowEditPage(RobotController* robot, PointStore* points, QWidget* parent = nullptr);
    ~FlowEditPage();
    void refreshPointCombo();

signals:
    void statusMessage(const QString&);

private slots:
    void onAddBlock(int type);
    void onDeleteBlock();
    void onMoveUp();
    void onMoveDown();
    void onBlockSelected(int row);
    void onApplyParams();
    void onNewFlow();
    void onSaveFlow();
    void onLoadFlow();
    void onPlay();
    void onStepOnce();
    void onPauseResume();
    void onStop();
    void onWorkerFinished(bool ok, const QString& msg);

private:
    void buildUi();
    void refreshBlockList();

    RobotController* m_robot;
    PointStore* m_points;
    Flow        m_flow;
    FlowStore   m_store;

    QThread       m_workerThread;
    FlowWorker*   m_worker = nullptr;
    bool          m_running = false;
    bool          m_isPaused = false;

    QLineEdit*    m_flowName;
    QListWidget*  m_blockList;
    int           m_currentRow = -1;

    QStackedWidget* m_paramStack;
    QComboBox*    m_pointCombo;
    QDoubleSpinBox* m_speed;
    QDoubleSpinBox* m_delaySec;
    QSpinBox*      m_speedPct;
    QSpinBox*      m_doIndex;
    QComboBox*     m_doValue;

    QPushButton* m_btnPlay;
    QPushButton* m_btnStep;
    QPushButton* m_btnPause;
    QPushButton* m_btnStop;
};
