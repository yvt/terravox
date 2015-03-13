#include "terrain.h"
#include <xmmintrin.h>
#include "cpu.h"

#if defined(__APPLE__)
#  define memcpy std::memcpy
#endif

Terrain::Terrain(const QSize &size) :
    size_(size)
{
    landform_.resize(size.width() * size.height());
    color_.resize(size.width() * size.height());
}

void Terrain::copyFrom(Terrain *t, QPoint dest, QRect src)
{
    Q_ASSERT(t);
    src = src.isValid() ? src : QRect(QPoint(0, 0), t->size());

    // clip
    if (dest.x() < 0) {
        src.setLeft(src.left() - dest.x());
        dest.setX(0);
    }
    if (dest.y() < 0) {
        src.setTop(src.top() - dest.y());
        dest.setY(0);
    }
    if (dest.x() + src.width() > size_.width()) {
        src.setWidth(size_.width() - dest.x());
    }
    if (dest.y() + src.height() > size_.height()) {
        src.setHeight(size_.height() - dest.y());
    }

    // TODO: clip by source boundary

    if (src.width() <= 0 || src.height() <= 0) {
        return;
    }

    Q_ASSERT(dest.x() >= 0);
    Q_ASSERT(dest.y() >= 0);
    Q_ASSERT(dest.x() + src.width() <= size().width());
    Q_ASSERT(dest.y() + src.height() <= size().height());
    Q_ASSERT(src.x() >= 0);
    Q_ASSERT(src.y() >= 0);
    Q_ASSERT(src.x() + src.width() <= t->size().width());
    Q_ASSERT(src.y() + src.height() <= t->size().height());


    for (int y = 0; y < src.height(); ++y) {
        memcpy(landform_.data() + (y + dest.y()) * size().width() + dest.x(),
                    t->landform_.data() + (y + src.y()) * t->size().width() + src.x(),
                    src.width() * sizeof(float));
        memcpy(color_.data() + (y + dest.y()) * size().width() + dest.x(),
                    t->color_.data() + (y + src.y()) * t->size().width() + src.x(),
                    src.width() * sizeof(quint32));
    }
}

void Terrain::quantize()
{
    int numCells = size_.width() * size_.height();
    auto *lf = landform_.data();
    auto maxValue = _mm_set1_ps(63.f);
    auto minValue = _mm_set1_ps(0.f);

    SseRoundingModeScope roundingModeScope(_MM_ROUND_NEAREST);
    (void)roundingModeScope;

    for (int i = 0; i < numCells; i += 4) {
        auto p = _mm_loadu_ps(lf + i);
        p = _mm_cvtepi32_ps(_mm_cvttps_epi32(p));
        p = _mm_max_ps(p, minValue);
        p = _mm_min_ps(p, maxValue);
        _mm_storeu_ps(lf + i, p);
    }
}

Terrain::~Terrain()
{

}

