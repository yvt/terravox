#ifndef SESSION_H
#define SESSION_H

#include <QObject>
#include <QList>
#include <QSharedPointer>
#include <QRect>

class Terrain;
class TerrainEdit;
class ToolController;

class Session : public QObject
{
    Q_OBJECT
public:
    explicit Session(QObject *parent = 0);
    ~Session();

    QSharedPointer<TerrainEdit> beginEdit();
    void endEdit();
    void cancelEdit();

    const QSharedPointer<Terrain> &terrain() const { return terrain_; }
    void setTerrain(QSharedPointer<Terrain> t) { terrain_ = t; }

    QSharedPointer<ToolController> tool() const { return tool_; }
    void setTool(QSharedPointer<ToolController> t);

    quint32 primaryColor() const { return primaryColor_; }
    void setPrimaryColor(quint32 color);

    bool isEditActive() const { return currentEdit_; }

    bool canUndo();
    bool canRedo();

    void terrainUpdate(const QRect &rect = QRect());

signals:
    void terrainUpdated(const QRect&);
    void toolChanged();
    void primaryColorChanged();

    void canUndoChanged(bool);
    void canRedoChanged(bool);

public slots:

    void undo();
    void redo();

private slots:

    void edited(const QRect&);

private:
    QSharedPointer<Terrain> terrain_;
    QList<QSharedPointer<TerrainEdit>> editHistory_;
    QSharedPointer<ToolController> tool_;
    int nextEditHistoryIndex_;
    QSharedPointer<TerrainEdit> currentEdit_;
    quint32 primaryColor_;
};

#endif // SESSION_H
