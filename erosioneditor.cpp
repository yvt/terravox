#include "erosioneditor.h"
#include "ui_erosioneditor.h"
#include "erosioneffect.h"

ErosionEditor::ErosionEditor(ErosionEffect *fx, QWidget *parent) :
    QWidget(parent),
    ui(new Ui::ErosionEditor),
    fx(fx)
{
    ui->setupUi(this);

    connect(fx, SIGNAL(parameterChanged(ErosionParameters)), SLOT(loadParameters()));
    connect(ui->strengthField, SIGNAL(valueChanged(int)), SLOT(storeParameters()));
    connect(ui->densityField, SIGNAL(valueChanged(int)), SLOT(storeParameters()));

    ui->strengthLabel->bindSpinBox(ui->strengthField);
    ui->densityLabel->bindSpinBox(ui->densityField);
}

ErosionEditor::~ErosionEditor()
{
    delete ui;
}

void ErosionEditor::loadParameters()
{
    auto p = fx->parameters();
    ui->strengthField->setValue(static_cast<int>(p.strength));
    ui->densityField->setValue(static_cast<int>(p.density));
}

void ErosionEditor::storeParameters()
{
    auto p = fx->parameters();
    p.strength = ui->strengthField->value();
    p.density = ui->densityField->value();
    fx->setParameters(p);
}
