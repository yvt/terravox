#include "brusheditor.h"
#include "ui_brusheditor.h"
#include <cmath>
#include "brushtool.h"

BrushEditor::BrushEditor(QSharedPointer<BrushTool> tool, QWidget *parent) :
    QWidget(parent),
    ui(new Ui::BrushEditor),
    tool(tool)
{
    Q_ASSERT(tool);

    ui->setupUi(this);

    ui->brushSizeLabel->bindSpinBox(ui->brushSize);
    ui->brushSizeLabel->setLogarithmic();

    ui->strengthLabel->bindSpinBox(ui->strength);

    ui->scaleLabel->bindSpinBox(ui->scaleField);
    ui->scaleLabel->setLogarithmic();

    connect(tool.data(), SIGNAL(parameterChanged()), SLOT(loadParameters()));
    loadParameters();

    connect(ui->shapeSelect, SIGNAL(currentIndexChanged(int)), SLOT(storeParameters()));
    connect(ui->brushSize, SIGNAL(valueChanged(int)), SLOT(storeParameters()));
    connect(ui->strength, SIGNAL(valueChanged(int)), SLOT(storeParameters()));
    connect(ui->pressureMode, SIGNAL(currentIndexChanged(int)), SLOT(storeParameters()));
    connect(ui->scaleField, SIGNAL(valueChanged(double)), SLOT(storeParameters()));

    connect(ui->randomizeButton, SIGNAL(clicked()), SLOT(randomizeSeed()));

    // "blur" doesn't have pressure mode control
    switch (tool->type()) {
    case BrushType::Blur:
    case BrushType::Smoothen:
        ui->pressureMode->setVisible(false);
        break;
    default:
        break;
    }
}

BrushEditor::~BrushEditor()
{
    disconnect();

    delete ui;
}

void BrushEditor::loadParameters()
{
    const auto &p = tool->parameters();

    switch(p.tipType) {
    case BrushTipType::Sphere:
        ui->shapeSelect->setCurrentIndex(0);
        break;
    case BrushTipType::Cylinder:
        ui->shapeSelect->setCurrentIndex(1);
        break;
    case BrushTipType::Square:
        ui->shapeSelect->setCurrentIndex(2);
        break;
    case BrushTipType::Cone:
        ui->shapeSelect->setCurrentIndex(3);
        break;
    case BrushTipType::Bell:
        ui->shapeSelect->setCurrentIndex(4);
        break;
    case BrushTipType::Mountains:
        ui->shapeSelect->setCurrentIndex(5);
        break;
    }
    ui->strength->setValue(static_cast<int>(std::round(p.strength)));
    ui->brushSize->setValue(p.size);
    switch (p.pressureMode) {
    case BrushPressureMode::AirBrush:
        ui->pressureMode->setCurrentIndex(0);
        break;
    case BrushPressureMode::Constant:
        ui->pressureMode->setCurrentIndex(1);
        break;
    case BrushPressureMode::Adjustable:
        ui->pressureMode->setCurrentIndex(2);
        break;
    }
    ui->scaleField->setValue(p.scale);

    updateUI();
}

void BrushEditor::storeParameters()
{
    BrushToolParameters p = tool->parameters();

    switch (ui->shapeSelect->currentIndex()) {
    case 0:
        p.tipType = BrushTipType::Sphere;
        break;
    case 1:
        p.tipType = BrushTipType::Cylinder;
        break;
    case 2:
        p.tipType = BrushTipType::Square;
        break;
    case 3:
        p.tipType = BrushTipType::Cone;
        break;
    case 4:
        p.tipType = BrushTipType::Bell;
        break;
    case 5:
        p.tipType = BrushTipType::Mountains;
        break;
    }
    p.strength = ui->strength->value();
    p.size = ui->brushSize->value();
    switch (ui->pressureMode->currentIndex()) {
    case 0:
        p.pressureMode = BrushPressureMode::AirBrush;
        break;
    case 1:
        p.pressureMode = BrushPressureMode::Constant;
        break;
    case 2:
        p.pressureMode = BrushPressureMode::Adjustable;
        break;
    }
    p.scale = ui->scaleField->value();
    tool->setParameters(p);

    updateUI();
}

void BrushEditor::updateUI()
{
    const auto &p = tool->parameters();
    ui->scaleLabel->setVisible(p.tipType == BrushTipType::Mountains);
    ui->scaleField->setVisible(p.tipType == BrushTipType::Mountains);
    ui->randomizeButton->setVisible(p.tipType == BrushTipType::Mountains);
}


void BrushEditor::randomizeSeed()
{
    BrushToolParameters p = tool->parameters();
    p.randomize();
    tool->setParameters(p);
}
