#include "session.h"
#include "terrainedit.h"
#include "terrain.h"

Session::Session(QObject *parent) :
    QObject(parent),
    nextEditHistoryIndex_(0),
    primaryColor_(0x7f7f7f)
{

}

Session::~Session()
{

}

QSharedPointer<TerrainEdit> Session::beginEdit()
{
    if (isEditActive()) {
        return QSharedPointer<TerrainEdit>();
    }
    Q_ASSERT(terrain_);


    QSharedPointer<TerrainEdit> edit = QSharedPointer<TerrainEdit>::create();

    connect(edit.data(), SIGNAL(edited(QRect)), SLOT(edited(QRect)));

    currentEdit_ = edit;

    emit canUndoChanged(canUndo());
    emit canRedoChanged(canRedo());

    return edit;
}

void Session::endEdit()
{
    Q_ASSERT(currentEdit_);
    Q_ASSERT(terrain_);

    auto edit = currentEdit_;
    edit->done();

    // add edit to history
    while (editHistory_.size() > nextEditHistoryIndex_) {
        editHistory_.pop_back();
    }

    editHistory_.push_back(edit);
    ++nextEditHistoryIndex_;

    if (editHistory_.size() > 100) {
        editHistory_.pop_front();;
        --nextEditHistoryIndex_;
    }

    disconnect(edit.data(), SIGNAL(edited(QRect)), this, SLOT(edited(QRect)));

    currentEdit_.reset();

    emit canUndoChanged(canUndo());
    emit canRedoChanged(canRedo());
}

void Session::cancelEdit()
{
    Q_ASSERT(currentEdit_);
    Q_ASSERT(terrain_);

    auto edit = currentEdit_;
    edit->undo(terrain_.data());

    emit terrainUpdated(edit->modifiedBounds());
    disconnect(edit.data(), SIGNAL(edited(QRect)), this, SLOT(edited(QRect)));

    currentEdit_.reset();

    emit canUndoChanged(canUndo());
    emit canRedoChanged(canRedo());
}

void Session::setTool(QSharedPointer<ToolController> t)
{
    tool_ = t;
    emit toolChanged();
}

void Session::setPrimaryColor(quint32 color)
{
    if (color == primaryColor_) {
        return;
    }
    primaryColor_ = color;
    emit primaryColorChanged();
}

bool Session::canUndo()
{
    return !isEditActive() && nextEditHistoryIndex_ > 0;
}

bool Session::canRedo()
{
    return !isEditActive() && nextEditHistoryIndex_ < editHistory_.size();
}

void Session::terrainUpdate(const QRect &rect)
{
    if (rect.isEmpty())
        emit terrainUpdated(QRect(QPoint(0, 0), terrain_->size()));
    else
        emit terrainUpdated(rect);
}

void Session::undo()
{
    if (!canUndo()) {
        return;
    }
    --nextEditHistoryIndex_;
    const auto& edit =  editHistory_[nextEditHistoryIndex_];
    edit->undo(terrain_.data());

    emit canUndoChanged(canUndo());
    emit canRedoChanged(canRedo());
    emit terrainUpdated(edit->modifiedBounds());
}

void Session::redo()
{
    if (!canRedo()) {
        return;
    }
    const auto &edit = editHistory_[nextEditHistoryIndex_];
    edit->apply(terrain_.data());
    ++nextEditHistoryIndex_;

    emit canUndoChanged(canUndo());
    emit canRedoChanged(canRedo());
    emit terrainUpdated(edit->modifiedBounds());
}

void Session::edited(const QRect &rt)
{
    emit terrainUpdated(rt);
}

