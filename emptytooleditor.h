#ifndef EMPTYTOOLEDITOR_H
#define EMPTYTOOLEDITOR_H

#include <QWidget>

namespace Ui {
class EmptyToolEditor;
}

class EmptyToolEditor : public QWidget
{
    Q_OBJECT

public:
    explicit EmptyToolEditor(QWidget *parent = 0);
    ~EmptyToolEditor();

private:
    Ui::EmptyToolEditor *ui;
};

#endif // EMPTYTOOLEDITOR_H
