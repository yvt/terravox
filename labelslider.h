#ifndef LABELSLIDER_H
#define LABELSLIDER_H

#include <QLabel>
#include <functional>

class QSpinBox;
class QDoubleSpinBox;

class LabelSlider : public QLabel
{
    Q_OBJECT

public:
    explicit LabelSlider(QWidget *parent = 0);
    ~LabelSlider();

    void bindSpinBox(QSpinBox *);
    void bindSpinBox(QDoubleSpinBox *);
    void setNonLinearConversion(const std::function<double(double)>&,
                                const std::function<double(double)>&);

protected:
    void mousePressEvent(QMouseEvent *) override;
    void mouseReleaseEvent(QMouseEvent *) override;
    void mouseMoveEvent(QMouseEvent *) override;

private:
    QSpinBox *spinBox_;
    QDoubleSpinBox *dspinBox_;
    bool dragging_;
    double dragStartValue_;
    QPoint dragStartPos_;
    std::function<double(double)> toNonLinearValue_;
    std::function<double(double)> toLinearValue_;
};

#endif // LABELSLIDER_H
