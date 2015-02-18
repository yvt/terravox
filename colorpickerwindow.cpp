#include "colorpickerwindow.h"
#include "ui_colorpickerwindow.h"

ColorPickerWindow::ColorPickerWindow(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::ColorPickerWindow)
{
    ui->setupUi(this);
    this->setWindowFlags(Qt::Tool);
}

ColorPickerWindow::~ColorPickerWindow()
{
    delete ui;
}

ColorPicker *ColorPickerWindow::colorPicker()
{
    return ui->widget;
}
