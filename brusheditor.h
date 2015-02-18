#ifndef BRUSHEDITOR_H
#define BRUSHEDITOR_H

#include <QWidget>
#include <QSharedPointer>

namespace Ui {
class BrushEditor;
}

class BrushTool;

class BrushEditor : public QWidget
{
    Q_OBJECT

public:
    explicit BrushEditor(QSharedPointer<BrushTool> tool, QWidget *parent = 0);
    ~BrushEditor();

private slots:
    void loadParameters();

    void storeParameters();

    void updateUI();

    void randomizeSeed();

private:
    Ui::BrushEditor *ui;
    QSharedPointer<BrushTool> tool;


};

#endif // BRUSHEDITOR_H
