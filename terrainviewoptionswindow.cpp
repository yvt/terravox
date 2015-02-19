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

    connect(ui->altitudeColorCheck, SIGNAL(toggled(bool)), SLOT(updateOptions()));
    connect(ui->useEdgeCheck, SIGNAL(toggled(bool)), SLOT(updateOptions()));
    connect(ui->aoCheck, SIGNAL(toggled(bool)), SLOT(updateOptions()));
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
    view->setViewOptions(options);
}
