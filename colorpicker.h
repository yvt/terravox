#ifndef COLORPICKER_H
#define COLORPICKER_H

#include <QWidget>
#include <QColor>

namespace Ui {
class ColorPicker;
}

class ColorPicker : public QWidget
{
    Q_OBJECT

public:
    explicit ColorPicker(QWidget *parent = 0);
    ~ColorPicker();

    QColor value() const { return value_; }

public slots:
    void setValue(const QColor&);

signals:
    void valueChanged(QColor);

private:
    Ui::ColorPicker *ui;
    QColor value_;
    enum class UpdatedField
    {
        None = 0,
        Rgb,
        Hsb,
        Hex,
        AnyOther
    };

    UpdatedField updating_;

private slots:
    void rgbChanged();
    void hsbChanged();
    void on_hexValue_editingFinished();
    void on_hexValue_textChanged(const QString &text);
};

#endif // COLORPICKER_H
