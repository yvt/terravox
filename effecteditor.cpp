#include "effecteditor.h"
#include "ui_effecteditor.h"
#include <QHBoxLayout>

EffectEditor::EffectEditor(QWidget *editor, QWidget *parent) :
    QWidget(parent),
    ui(new Ui::EffectEditor)
{
    ui->setupUi(this);

    auto *layout = new QHBoxLayout();
    layout->addWidget(editor);
    layout->setMargin(0);
    ui->editorPanel->setLayout(layout);

    connect(ui->applyButton, SIGNAL(clicked()),
            SIGNAL(applyEffect()));
    connect(ui->revertButton, SIGNAL(clicked()),
            SIGNAL(revertEffect()));
}

EffectEditor::~EffectEditor()
{
    delete ui;
}
