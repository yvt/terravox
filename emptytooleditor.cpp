#include "emptytooleditor.h"
#include "ui_emptytooleditor.h"

EmptyToolEditor::EmptyToolEditor(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::EmptyToolEditor)
{
    ui->setupUi(this);
}

EmptyToolEditor::~EmptyToolEditor()
{
    delete ui;
}
