#ifndef COLORVIEW_H
#define COLORVIEW_H

#include <QWidget>
#include <QAbstractButton>
#include <QColor>

class ColorView : public QAbstractButton
{
    Q_OBJECT
public:
    explicit ColorView(QWidget *parent = 0);
    ~ColorView();

    QColor value() const { return value_; }

public slots:
    void setValue(const QColor&);

protected:
    void paintEvent(QPaintEvent *) override;
    void dragEnterEvent(QDragEnterEvent *) override;
    void dropEvent(QDropEvent *) override;
    void mousePressEvent(QMouseEvent *e) override;
    void mouseMoveEvent(QMouseEvent *e) override;

signals:
    void valueChanged(QColor);

private:
    QColor value_;
    QPoint dragStartPos_;
};

#endif // COLORVIEW_H
