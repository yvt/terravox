#include "labelslider.h"
#include <QSpinBox>
#include <QMouseEvent>
#include <cmath>

LabelSlider::LabelSlider(QWidget *parent) :
    QLabel(parent),
    spinBox_(nullptr),
    dspinBox_(nullptr),
    dragging_(false),
    toNonLinearValue_([](double i){return i;}),
    toLinearValue_([](double i){return i;})
{
    setCursor(QCursor(Qt::SizeHorCursor));
    setStyleSheet("color: #ff8f10; text-decoration: underline;");
}
LabelSlider::~LabelSlider()
{
}

void LabelSlider::bindSpinBox(QSpinBox *spinBox)
{
    spinBox_ = spinBox;
}
void LabelSlider::bindSpinBox(QDoubleSpinBox *spinBox)
{
    dspinBox_ = spinBox;
}

void LabelSlider::setNonLinearConversion(const std::function<double (double)> &forward,
                                         const std::function<double (double)> &backward)
{
    toNonLinearValue_ = forward;
    toLinearValue_ = backward;
}

void LabelSlider::mousePressEvent(QMouseEvent *e)
{
    if (e->button() != Qt::LeftButton || (!spinBox_ && !dspinBox_)) {
        return;
    }

    dragging_ = true;
    if (spinBox_)
        dragStartValue_ = toNonLinearValue_(spinBox_->value());
    else if (dspinBox_)
        dragStartValue_ = toNonLinearValue_(dspinBox_->value());
    dragStartPos_ = e->pos();
}

void LabelSlider::mouseReleaseEvent(QMouseEvent *e)
{
    if (e->button() != Qt::LeftButton) {
        return;
    }

    dragging_ = false;
}

void LabelSlider::mouseMoveEvent(QMouseEvent *e)
{
    if (dragging_) {
        QPoint pt = e->pos();
        pt -= dragStartPos_;
        int delta = pt.x() - pt.y();
        double nl = dragStartValue_ + delta * 0.23;
        if (spinBox_)
            spinBox_->setValue(toLinearValue_(nl));
        else if (dspinBox_)
            dspinBox_->setValue(toLinearValue_(nl));
    }
}
