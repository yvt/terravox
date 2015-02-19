#ifndef TERRAINVIEW_P_H
#define TERRAINVIEW_P_H

#include <QObject>
#include "terrainview.h"
#include <QVector3D>
#include <QVector2D>
#include <QVector>
#include "temporarybuffer.h"
#include <cmath>

static const float depthBufferBias = -1.e-1f; // intentionally add bias to prevent flickering

struct TerrainView::SceneDefinition
{
    QVector3D eye;
    QVector3D cameraDir;
    float viewWidth, viewHeight;
};

struct RangeIterator :
        public std::iterator<std::forward_iterator_tag, int>
{
    mutable int index;
    RangeIterator(int i) : index(i) { }
    bool operator == (const RangeIterator &o) const { return index == o.index; }
    bool operator != (const RangeIterator &o) const { return index != o.index; }
    RangeIterator& operator ++ () { ++index; return *this; }
    int& operator *() { return index; }
};

class TerrainView::TerrainViewPrivate
{
    TerrainView *q_ptr;
    Q_DECLARE_PUBLIC(TerrainView)

    QVector3D centerPos_;
    float yaw_, pitch_;
    float scale_;

    QPoint dragPos_;
    bool dragging_;
    bool dragShiftMode_;
    QVector3D dragStartCenterPos_;
    float dragStartYaw_;
    float dragStartPitch_;

    QVector<float> depthImage_;
    QVector<quint32> transposedImage_;
    std::unique_ptr<QImage> image_;

    QSharedPointer<Terrain> terrain_;

    // height map for ambient occlusion volume (linear interpolated)
    QVector<float> aoMap_;
    QRect aoMapDirtyRect_;

    SceneDefinition sceneDef;
    QVector3D rightVector;
    QVector3D upVector;

    QVector2D cameraDir2D;
    float zBias;

    TerrainViewOptions viewOptions;

    template <class Colorizer>
    void renderLandform(Colorizer&& colorizer);

    void updateAmbientOcclusion();
    void applyAmbientOcclusion();
    void applySharpeningFilter();

    class DrawingContext;

    void emitTerrainPaint();

public:
    TerrainViewPrivate(TerrainView *q) :
        q_ptr(q),
        centerPos_(256, 256, 64),
        yaw_(M_PI / 4.f),
        pitch_(.3f),
        scale_(2.f),
        dragging_(false)
    {
    }

    QPointF rayCastByDepthBuffer(const QPoint &pt);

    void terrainUpdate(const QRect &bounds);

    QImage &render(QSize size, const SceneDefinition &def);
};

#endif // TERRAINVIEW_P_H

