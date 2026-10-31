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

class PointManagerPage : public QWidget
{
    Q_OBJECT
public:
    PointManagerPage(RobotController* robot, PointStore* points, QWidget* parent = nullptr);

signals:
    void statusMessage(const QString&);
    void pointsChanged();

private slots:
    void onNewPoint();
    void onDeletePoint();
    void onReadCurrent();
    void onApplyEdit();       // 把右侧表单写入当前选中行
    void onSelectionChanged();

private:
    void buildUi();
    void refreshTable();
    void loadFromStore();
    bool saveStore();

    RobotController* m_robot;
    PointStore*      m_store;

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
};
