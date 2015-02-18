#ifndef EROSIONEDITOR_H
#define EROSIONEDITOR_H

#include <QWidget>

namespace Ui {
class ErosionEditor;
}
class ErosionEffect;

class ErosionEditor : public QWidget
{
    Q_OBJECT

public:
    explicit ErosionEditor(ErosionEffect *fx, QWidget *parent = 0);
    ~ErosionEditor();

private slots:
    void loadParameters();
    void storeParameters();

private:
    Ui::ErosionEditor *ui;
    ErosionEffect *fx;
};

#endif // EROSIONEDITOR_H
