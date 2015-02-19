#include "terrainedit.h"
#include "terrain.h"
#include <QRect>


inline uint qHash(const QPoint &key, uint seed)
{
    return qHash(key.x(), seed) ^ (qHash(key.y(), seed) >> 8 | qHash(~key.y(), seed) << 24);
}

TerrainEdit::TerrainEdit(QObject *parent) :
    QObject(parent),
    done_(false)
{

}

TerrainEdit::~TerrainEdit()
{

}

void TerrainEdit::beginEdit(const QRect &r, Terrain *t)
{
    Q_ASSERT(!done_);
    Q_ASSERT(t);
    if (r.isEmpty()) {
        return;
    }
    Q_ASSERT(r.left() >= 0); Q_ASSERT(r.top() >= 0);
    Q_ASSERT(r.x() + r.width() <= t->size().width());
    Q_ASSERT(r.y() + r.height() <= t->size().height());

    int x1 = r.x() >> SubeditSizeBits;
    int y1 = r.y() >> SubeditSizeBits;
    int x2 = (r.x() + r.width() + SubeditSize - 1) >> SubeditSizeBits;
    int y2 = (r.y() + r.height() + SubeditSize - 1) >> SubeditSizeBits;
    for (int x = x1; x < x2; ++x) {
        for (int y = y1; y < y2; ++y) {
            QPoint block(x, y);
            TerrainSubedit &subedit = subedits_[block];
            if (!subedit.before) {
                // initialize subedit
                subedit.position = QPoint(x << SubeditSizeBits, y << SubeditSizeBits);
                subedit.before = std::make_shared<Terrain>(QSize(SubeditSize, SubeditSize));
                subedit.after = std::make_shared<Terrain>(QSize(SubeditSize, SubeditSize));
                subedit.before->copyFrom(t, QPoint(0, 0), QRect(subedit.position, QSize(SubeditSize, SubeditSize)));
                subedit.editing = false;
            }
            subedit.editing = true;
        }
    }
}

void TerrainEdit::endEdit(Terrain *t)
{
    Q_ASSERT(!done_);
    Q_ASSERT(t);

    QRect rt;
    for (TerrainSubedit& e: subedits_) {
        if (e.editing) {
            if (rt.isEmpty()) {
                rt = QRect(e.position.x(), e.position.y(), SubeditSize, SubeditSize);
            } else {
                rt = rt.united(QRect(e.position.x(), e.position.y(), SubeditSize, SubeditSize));
            }
            e.after->copyFrom(t, QPoint(0, 0), QRect(e.position.x(), e.position.y(), SubeditSize, SubeditSize));
            e.editing = false;
        }
    }

    if (!rt.isEmpty()) {
        emit edited(rt);
    }
}

void TerrainEdit::done()
{
    done_ = true;
}

void TerrainEdit::apply(Terrain *t)
{
    Q_ASSERT(t);
    for (TerrainSubedit& e: subedits_) {
        t->copyFrom(e.after.get(), e.position);
    }
}

void TerrainEdit::undo(Terrain *t)
{
    Q_ASSERT(t);
    for (TerrainSubedit& e: subedits_) {
        t->copyFrom(e.before.get(), e.position);
    }
}

QRect TerrainEdit::modifiedBounds()
{
    QRect rt;
    for (TerrainSubedit& e: subedits_) {
        if (rt.isEmpty()) {
            rt = QRect(e.position.x(), e.position.y(), SubeditSize, SubeditSize);
        } else {
            rt = rt.united(QRect(e.position.x(), e.position.y(), SubeditSize, SubeditSize));
        }
    }
    return rt;
}

void TerrainEdit::copyBeforeTo(const QRect &srcRect, Terrain *after, Terrain *copyTo)
{
    Q_ASSERT(after);
    Q_ASSERT(copyTo);

    QRect r = srcRect;
    Q_ASSERT(r.x() >= 0); Q_ASSERT(r.y() >= 0);
    Q_ASSERT(r.x() + r.width() <= after->size().width());
    Q_ASSERT(r.y() + r.height() <= after->size().height());

    // TODO: clip copy destination size by srcRect

    int x1 = r.x() >> SubeditSizeBits;
    int y1 = r.y() >> SubeditSizeBits;
    int x2 = (r.x() + r.width() + SubeditSize - 1) >> SubeditSizeBits;
    int y2 = (r.y() + r.height() + SubeditSize - 1) >> SubeditSizeBits;

    for (int x = x1; x < x2; ++x) {
        for (int y = y1; y < y2; ++y) {
            QPoint block(x, y);
            auto it = subedits_.find(block);
            if (it == subedits_.end()) {
                // unmodified
                copyTo->copyFrom(after, QPoint((x << SubeditSizeBits) - srcRect.x(), (y << SubeditSizeBits) - srcRect.y()),
                                 QRect(x << SubeditSizeBits, y << SubeditSizeBits,
                                       SubeditSize, SubeditSize));
            } else {
                // modified
                TerrainSubedit &subedit = *it;
                copyTo->copyFrom(subedit.before.get(),
                                 QPoint((x << SubeditSizeBits) - srcRect.x(), (y << SubeditSizeBits) - srcRect.y()));
            }
        }
    }

}

uint TerrainEdit::size() const
{
    return subedits_.size() * SubeditSize * SubeditSize * 8;
}
