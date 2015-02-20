#include "terrainviewoptionswindow.h"
#include "ui_terrainviewoptionswindow.h"
#include "terrainview.h"

TerrainViewOptionsWindow::TerrainViewOptionsWindow(TerrainView *view, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::TerrainViewOptionsWindow),
    view(view)
{
    ui->setupUi(this);

    this->setWindowFlags(Qt::Tool);
    this->setFixedSize(this->size());

    const TerrainViewOptions &options = view->viewOptions();
    ui->altitudeColorCheck->setChecked(options.colorizeAltitude);
    ui->useEdgeCheck->setChecked(options.showEdges);
    ui->aoCheck->setChecked(options.ambientOcclusion);
    ui->aoSlider->setValue(options.ambientOcclusionStrength * 1000.f);
    ui->axisesCheck->setChecked(options.axises);

    connect(ui->altitudeColorCheck, SIGNAL(toggled(bool)), SLOT(updateOptions()));
    connect(ui->useEdgeCheck, SIGNAL(toggled(bool)), SLOT(updateOptions()));
    connect(ui->aoCheck, SIGNAL(toggled(bool)), SLOT(updateOptions()));
    connect(ui->aoSlider, SIGNAL(valueChanged(int)), SLOT(updateOptions()));
    connect(ui->axisesCheck, SIGNAL(toggled(bool)), SLOT(updateOptions()));
}

TerrainViewOptionsWindow::~TerrainViewOptionsWindow()
{
    delete ui;
}

void TerrainViewOptionsWindow::updateOptions()
{
    TerrainViewOptions options = view->viewOptions();
    options.colorizeAltitude = ui->altitudeColorCheck->isChecked();
    options.showEdges = ui->useEdgeCheck->isChecked();
    options.ambientOcclusion = ui->aoCheck->isChecked();
    options.ambientOcclusionStrength = ui->aoSlider->value() / 1000.f;
    options.axises = ui->axisesCheck->isChecked();

    view->setViewOptions(options);
}
