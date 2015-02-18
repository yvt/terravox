#include "colorview.h"
#include <QPainter>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QPoint>
#include <QMouseEvent>
#include <QApplication>
#include <QDrag>
#include <QPixmap>

ColorView::ColorView(QWidget *parent) : QAbstractButton(parent)
{

}

ColorView::~ColorView()
{

}

void ColorView::setValue(const QColor &col)
{
    if (col.rgb() == value_.rgb()) {
        return;
    }
    value_ = col;
    update();
    emit valueChanged(col);
}

void ColorView::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    QSize size = this->size();

    for (int x = 0; x < size.width(); x += 10) {
        for (int y = 0; y < size.height(); y += 10) {
             p.fillRect(x, y, 10, 10, ((x + y) / 10 & 1) ? QColor(255, 255, 255) : QColor(192, 192, 192));
        }
    }

    p.fillRect(0, 0, size.width(), size.height(), value_);
    auto v2 = value_; v2.setAlphaF(1.f);
    p.fillRect(0, 0, size.width() / 2, size.height(), v2);

    p.setPen(QColor(0, 0, 0));
    p.drawRect(0, 0, size.width() - 1, size.height() - 1);
    p.setPen(QColor(255, 255, 255));
    p.drawRect(1, 1, size.width() - 3, size.height() - 3);

}

void ColorView::dragEnterEvent(QDragEnterEvent *e)
{
    if (e->mimeData()->hasColor())
        e->acceptProposedAction();
}

void ColorView::dropEvent(QDropEvent *e)
{
    if (e->mimeData()->hasColor()) {
        auto color = qvariant_cast<QColor>(e->mimeData()->colorData());
        setValue(color);
        e->acceptProposedAction();
    }
}

void ColorView::mousePressEvent(QMouseEvent *e)
{
    dragStartPos_ = e->pos();

    QAbstractButton::mousePressEvent(e);
}

void ColorView::mouseMoveEvent(QMouseEvent *e)
{
    if (e->buttons() & Qt::LeftButton) {
        if ((e->pos() - dragStartPos_).manhattanLength() >= QApplication::startDragDistance()) {
            QDrag *drag = new QDrag(this);
            QMimeData *mimeData = new QMimeData;

            mimeData->setColorData(value_);
            drag->setMimeData(mimeData);

            QPixmap pix(20, 20);
            auto size = pix.size();
            {
                QPainter p(&pix);

                for (int x = 0; x < size.width(); x += 5) {
                    for (int y = 0; y < size.height(); y += 5) {
                         p.fillRect(x, y, 5, 5, ((x + y) / 5 & 1) ? QColor(255, 255, 255) : QColor(192, 192, 192));
                    }
                }

                p.fillRect(0, 0, size.width(), size.height(), value_);
                auto v2 = value_; v2.setAlphaF(1.f);
                p.fillRect(0, 0, size.width() / 2, size.height(), v2);

                p.setPen(QColor(0, 0, 0));
                p.drawRect(0, 0, size.width() - 1, size.height() - 1);
                p.setPen(QColor(255, 255, 255));
                p.drawRect(1, 1, size.width() - 3, size.height() - 3);
            }

            drag->setPixmap(pix);
            drag->exec(Qt::CopyAction | Qt::MoveAction);
            return;
        }
    }

    QAbstractButton::mouseMoveEvent(e);
}

