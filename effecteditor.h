#ifndef EFFECTEDITOR_H
#define EFFECTEDITOR_H

#include <QWidget>

namespace Ui {
class EffectEditor;
}

class EffectEditor : public QWidget
{
    Q_OBJECT

public:
    explicit EffectEditor(QWidget *editor, QWidget *parent = 0);
    ~EffectEditor();

signals:
    void applyEffect();
    void revertEffect();

private:
    Ui::EffectEditor *ui;
};

#endif // EFFECTEDITOR_H
