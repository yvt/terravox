#include "effectcontroller.h"
#include "effecteditor.h"
#include <QMessageBox>
#include <QApplication>
#include "terrain.h"
#include "terrainedit.h"
#include "session.h"
#include "emptytooleditor.h"

EffectController::EffectController(QObject *parent) :
    ToolController(parent),
    editor(nullptr),
    session(nullptr)
{

}

EffectController::~EffectController()
{

}

QWidget *EffectController::createEditor(Session *session)
{
    Q_ASSERT(!this->editor);

    auto *innerEditor = createEffectEditor(session);
    if (!innerEditor)
        innerEditor = new EmptyToolEditor();
    auto *editor = new EffectEditor(innerEditor);
    connect(editor, SIGNAL(destroyed(QObject*)), SLOT(editorDestroyed(QObject*)));
    connect(editor, SIGNAL(applyEffect()), SLOT(apply()));
    connect(editor, SIGNAL(revertEffect()), SLOT(revert()));
    this->session =  session;
    this->editor = editor;

    Q_ASSERT(!previewEdit);
    previewEdit = session->beginEdit();

    preview();

    return editor;
}

bool EffectController::leave(std::function<void (bool)> callback)
{
    Q_ASSERT(editor);
    QMessageBox *msgbox = new QMessageBox(
                QMessageBox::NoIcon, QApplication::applicationName(), tr("Effect is being edited."),
                QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel, editor->window());
    msgbox->setInformativeText(tr("Apply the effect?"));
    msgbox->setWindowFlags(Qt::Sheet);
    msgbox->setDefaultButton(QMessageBox::Save);
    msgbox->show();

    auto self = sharedFromThis();

    connect(msgbox, &QMessageBox::finished, [this, msgbox, callback, self](int button) {
        switch (button) {
        case QMessageBox::Yes:
            QApplication::setOverrideCursor(Qt::WaitCursor);
            apply();
            QApplication::restoreOverrideCursor();
            callback(true);
            break;
        case QMessageBox::No:
            QApplication::setOverrideCursor(Qt::WaitCursor);
            revert();
            QApplication::restoreOverrideCursor();
            callback(true);
            break;
        case QMessageBox::Cancel:
            callback(false);
            break;
        }
        //msgbox->deleteLater();
    });
    return false;
}

void EffectController::preview()
{
    Q_ASSERT(session);

    applyEffect(session->terrain().data(), previewEdit, session);
}

void EffectController::apply()
{
    Q_ASSERT(session);
    Q_ASSERT(previewEdit);

    preview();

    session->endEdit();
    previewEdit.reset();

    emit applied();
    emit done();
}

void EffectController::revert()
{
    Q_ASSERT(session);
    Q_ASSERT(previewEdit);

    session->cancelEdit();
    previewEdit.reset();

    emit reverted();
    emit done();
}

void EffectController::editorDestroyed(QObject *obj)
{
    if (editor == obj) {
        editor = nullptr;
        session = nullptr;
    }
}
