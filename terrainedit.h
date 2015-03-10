#ifndef TERRAINEDIT_H
#define TERRAINEDIT_H

#include <QObject>
#include <QPoint>
#include <QHash>
#include <QRect>

#include <memory>

class Terrain;


struct TerrainSubedit
{
    std::shared_ptr<Terrain> before, after;
    QPoint position;
    bool editing;
};

class TerrainEdit : public QObject
{
    Q_OBJECT
public:
    explicit TerrainEdit(QObject *parent = 0);
    ~TerrainEdit();

    void beginEdit(const QRect&, Terrain *);
    void endEdit(Terrain *);

    void done();

    void apply(Terrain *);
    void undo(Terrain *);

    QRect modifiedBounds();

    /** Generates "before" image and writes it to `copyTo`. */
    void copyBeforeTo(const QRect &srcRect, Terrain *after, Terrain *copyTo);

    uint size() const;

    static constexpr int SubeditSizeBits = 5;
    static constexpr int SubeditSize = 1 << SubeditSizeBits;

signals:
    void edited(QRect);

public slots:

private:
    bool done_;
    QHash<QPoint, TerrainSubedit> subedits_;
};

#endif // TERRAINEDIT_H
