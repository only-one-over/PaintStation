#pragma once

#include <QWidget>
#include "PointStore.h"

class QTableWidget;
class QLineEdit;
class QComboBox;
class QDoubleSpinBox;
class QPushButton;
class QLabel;

class RobotController;
class AppConfig;
class QThread;

class PointManagerPage : public QWidget
{
    Q_OBJECT
public:
    PointManagerPage(RobotController* robot, PointStore* points, AppConfig* cfg, QWidget* parent = nullptr);
    ~PointManagerPage();

signals:
    void statusMessage(const QString&);
    void pointsChanged();

private slots:
    void onNewPoint();
    void onDeletePoint();
    void onReadCurrent();
    void onApplyEdit();       // 把右侧表单写入当前选中行
    void onSelectionChanged();
    void onMoveToPoint();

private:
    void buildUi();
    void refreshTable();
    void loadFromStore();
    bool saveStore();

    RobotController* m_robot;
    PointStore*      m_store;
    AppConfig* m_cfg;
    QThread* m_moveThread = nullptr;
    QPushButton* m_btnMove = nullptr;

    QTableWidget*  m_table;
    QLineEdit*     m_name;
    QComboBox*     m_type;
    QDoubleSpinBox* m_j[6];
    QDoubleSpinBox* m_xyz[3];
    QDoubleSpinBox* m_abc[3];
    QLineEdit*     m_tool;
    QLineEdit*     m_comment;
    QPushButton*   m_btnReadCur;
    QPushButton*   m_btnNew;
    QPushButton*   m_btnDel;
    QPushButton*   m_btnApply;

    int m_currentRow = -1;
    bool m_canSave = true;
};
