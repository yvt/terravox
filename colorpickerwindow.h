#ifndef COLORPICKERWINDOW_H
#define COLORPICKERWINDOW_H

#include <QDialog>

namespace Ui {
class ColorPickerWindow;
}

class ColorPicker;

class ColorPickerWindow : public QDialog
{
    Q_OBJECT

public:
    explicit ColorPickerWindow(QWidget *parent = 0);
    ~ColorPickerWindow();

    ColorPicker *colorPicker();

private:
    Ui::ColorPickerWindow *ui;
};

#endif // COLORPICKERWINDOW_H
