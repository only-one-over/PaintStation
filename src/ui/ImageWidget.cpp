#include "ImageWidget.h"

#include <QPainter>
#include <QMouseEvent>
#include <QDebug>

ImageWidget::ImageWidget(QWidget* parent) : QWidget(parent)
{
    setMinimumSize(640, 480);
    setStyleSheet("background:black;");
    setCursor(Qt::CrossCursor);
}

void ImageWidget::showImage(const QImage& img)
{
    m_img = img;
    update();
}

void ImageWidget::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.fillRect(rect(), Qt::black);
    if (m_img.isNull()) return;

    // 等比缩放居中
    QSize rs = m_img.size().scaled(size(), Qt::KeepAspectRatio);
    m_drawRect = QRect((width()-rs.width())/2, (height()-rs.height())/2, rs.width(), rs.height());
    p.drawImage(m_drawRect, m_img);

    // 画最后点击位置
    if (!m_lastClick.isNull()) {
        double sx = (double)m_img.width()  / m_drawRect.width();
        double sy = (double)m_img.height() / m_drawRect.height();
        int dx = m_drawRect.x() + (int)(m_lastClick.x()/sx);
        int dy = m_drawRect.y() + (int)(m_lastClick.y()/sy);
        p.setPen(Qt::red);
        p.drawLine(dx-6,dy,dx+6,dy);
        p.drawLine(dx,dy-6,dx,dy+6);
    }
}

void ImageWidget::mousePressEvent(QMouseEvent* e)
{
    if (m_img.isNull() || !m_drawRect.contains(e->pos())) return;
    double sx = (double)m_img.width()  / m_drawRect.width();
    double sy = (double)m_img.height() / m_drawRect.height();
    int u = (int)((e->pos().x() - m_drawRect.x()) * sx);
    int v = (int)((e->pos().y() - m_drawRect.y()) * sy);
    if (u < 0 || v < 0 || u >= m_img.width() || v >= m_img.height()) return;
    m_lastClick = QPoint(u,v);
    update();
    emit pixelClicked(u,v);
}

void ImageWidget::resizeEvent(QResizeEvent*) { update(); }
