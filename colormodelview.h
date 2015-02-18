#ifndef COLORMODELVIEW_H
#define COLORMODELVIEW_H

#include <QWidget>
#include <QPixmap>

class ColorModelView : public QWidget
{
    Q_OBJECT
public:
    explicit ColorModelView(QWidget *parent = 0);
    ~ColorModelView();

    QColor color() const { return value_; }

public slots:
    void setValue(const QColor&);

protected:
    void paintEvent(QPaintEvent *) override;
    void mousePressEvent(QMouseEvent *) override;
    void mouseReleaseEvent(QMouseEvent *) override;
    void mouseMoveEvent(QMouseEvent *) override;

    QRect mainAreaBounds();
    QRect sideAreaBounds();

signals:
    void valueChanged(QColor);

private:
    enum class DragMode
    {
        None,
        Main,
        Side
    };

    QColor value_;
    DragMode drag_;

    QPixmap mainImage_;
    QPixmap sideImage_;
};

#endif // COLORMODELVIEW_H
