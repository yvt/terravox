#ifndef TERRAINVIEWOPTIONSWINDOW_H
#define TERRAINVIEWOPTIONSWINDOW_H

#include <QDialog>

namespace Ui {
class TerrainViewOptionsWindow;
}

class TerrainView;

class TerrainViewOptionsWindow : public QDialog
{
    Q_OBJECT

public:
    explicit TerrainViewOptionsWindow(TerrainView *view, QWidget *parent = 0);
    ~TerrainViewOptionsWindow();

private:
    Ui::TerrainViewOptionsWindow *ui;
    TerrainView *view;

private slots:
    void updateOptions();
};

#endif // TERRAINVIEWOPTIONSWINDOW_H
