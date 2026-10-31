#pragma once

#include <QWidget>
#include <QImage>
#include <QPoint>

// 可显示 OpenCV 图像、响应鼠标点击的控件
// 点击后发出 pixelClicked(u, v)，u/v 是原图坐标（不是控件坐标）
class ImageWidget : public QWidget
{
    Q_OBJECT
public:
    explicit ImageWidget(QWidget* parent = nullptr);

    void showImage(const QImage& img);
    QImage currentImage() const { return m_img; }

signals:
    void pixelClicked(int u, int v);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    QImage m_img;
    QRect  m_drawRect;   // 实际绘制区域（保持比例）
    QPoint m_lastClick;
};
