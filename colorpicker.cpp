#include "colorpicker.h"
#include "ui_colorpicker.h"

ColorPicker::ColorPicker(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::ColorPicker),
    updating_(UpdatedField::None)
{
    ui->setupUi(this);

    ui->redLabel->bindSpinBox(ui->redField);
    ui->greenLabel->bindSpinBox(ui->greenField);
    ui->blueLabel->bindSpinBox(ui->blueField);
    ui->hueLabel->bindSpinBox(ui->hueField);
    ui->saturationLabel->bindSpinBox(ui->saturationField);
    ui->brightnessLabel->bindSpinBox(ui->brightnessField);

    connect(ui->hueField, SIGNAL(valueChanged(int)),
            SLOT(hsbChanged()));
    connect(ui->saturationField, SIGNAL(valueChanged(int)),
            SLOT(hsbChanged()));
    connect(ui->brightnessField, SIGNAL(valueChanged(int)),
            SLOT(hsbChanged()));
    connect(ui->redField, SIGNAL(valueChanged(int)),
            SLOT(rgbChanged()));
    connect(ui->greenField, SIGNAL(valueChanged(int)),
            SLOT(rgbChanged()));
    connect(ui->blueField, SIGNAL(valueChanged(int)),
            SLOT(rgbChanged()));

    connect(ui->colorPanel, SIGNAL(valueChanged(QColor)),
            SLOT(setValue(QColor)));
    connect(this, SIGNAL(valueChanged(QColor)),
            ui->colorPanel, SLOT(setValue(QColor)));

    connect(ui->colorModelBox, SIGNAL(valueChanged(QColor)),
            SLOT(setValue(QColor)));
    connect(this, SIGNAL(valueChanged(QColor)),
            ui->colorModelBox, SLOT(setValue(QColor)));

}

ColorPicker::~ColorPicker()
{
    delete ui;
}

void ColorPicker::setValue(const QColor &col)
{
    if (col.rgb() == value_.rgb()) {
        return;
    }
    value_ = col;

    auto o = updating_;
    if (updating_ == UpdatedField::None) {
        updating_ = UpdatedField::AnyOther;
    }

    if (updating_ != UpdatedField::Hsb) {
        ui->hueField->setValue(value_.hsvHue());
        ui->saturationField->setValue(value_.hsvSaturation());
        ui->brightnessField->setValue(value_.value());
    }

    if (updating_ != UpdatedField::Rgb) {
        ui->redField->setValue(value_.red());
        ui->greenField->setValue(value_.green());
        ui->blueField->setValue(value_.blue());
    }

    if (updating_ != UpdatedField::Hex) {
        ui->hexValue->setText(value_.name(QColor::HexRgb).mid(1));
    }

    updating_ = o;

    emit valueChanged(col);
}

void ColorPicker::rgbChanged()
{
    if (updating_ != UpdatedField::None)
        return;
    updating_ = UpdatedField::Rgb;
    setValue(QColor::fromRgb(ui->redField->value(), ui->greenField->value(), ui->blueField->value()));
    updating_ = UpdatedField::None;
}

void ColorPicker::hsbChanged()
{
    if (updating_ != UpdatedField::None)
        return;
    updating_ = UpdatedField::Hsb;
    setValue(QColor::fromHsv(ui->hueField->value(), ui->saturationField->value(), ui->brightnessField->value()));
    updating_ = UpdatedField::None;
}

void ColorPicker::on_hexValue_editingFinished()
{
    updating_ = UpdatedField::Hex;
    ui->hexValue->setText(value_.name(QColor::HexRgb).mid(1));
    updating_ = UpdatedField::None;
}

void ColorPicker::on_hexValue_textChanged(const QString &text)
{
    if (updating_ != UpdatedField::None)
        return;

    updating_ = UpdatedField::Hex;
    QColor color("#" + text);
    if (color.isValid()) {
        setValue(color);
    }
    updating_ = UpdatedField::None;
}
