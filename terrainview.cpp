#include "terrainview.h"
#include <QPaintEvent>
#include <QPainter>
#include "terrain.h"
#include <emmintrin.h>
#include "terraingenerator.h"
#include <QVector3D>
#include <QVector2D>
#include <cmath>
#include <smmintrin.h>
#include <QtConcurrent>
#include <QDebug>
#include "terrainviewoptionswindow.h"

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

    SceneDefinition sceneDef;
    QVector3D rightVector;
    QVector3D upVector;

    QVector2D cameraDir2D;
    float zBias;

    TerrainViewOptions viewOptions;

    static void transposeImage(quint32 *dest, QSize size, const quint32 *src)
    {
        Q_ASSUME((size.width() & 3) == 0);
        Q_ASSUME((size.height() & 3) == 0);

        const int width = size.width();
        const int height = size.height();

        QtConcurrent::blockingMap(RangeIterator(0), RangeIterator(height >> 2), [=](int block) {
            int y = block << 2;
            {
                auto *destptr = reinterpret_cast<float *>(dest + y * width);
                auto *srcptr = reinterpret_cast<const float *>(src + y);
                for (int x = 0; x < width; x += 4) {
                    auto p1 = _mm_loadu_ps(srcptr); srcptr += height;
                    auto p2 = _mm_loadu_ps(srcptr); srcptr += height;
                    auto p3 = _mm_loadu_ps(srcptr); srcptr += height;
                    auto p4 = _mm_loadu_ps(srcptr); srcptr += height;

                    _MM_TRANSPOSE4_PS(p1, p2, p3, p4);

                    auto *destptr2 = destptr;
                    _mm_storeu_ps(destptr2, p1); destptr2 += width;
                    _mm_storeu_ps(destptr2, p2); destptr2 += width;
                    _mm_storeu_ps(destptr2, p3); destptr2 += width;
                    _mm_storeu_ps(destptr2, p4); destptr2 += width;

                    destptr += 4;
                }
            }
        });
    }


    static bool pushToBoundary(QVector2D &v, const QVector2D &rayDir, const QVector2D &lineNormal, float linePos, bool &first)
    {
        float d = QVector2D::dotProduct(v, lineNormal) - linePos;
        float dt = QVector2D::dotProduct(rayDir, lineNormal);
        Q_ASSUME(dt != 0.f);
        bool always = first;
        first = false;
        if (d < 0.f || always) {
            v -= rayDir * (d / dt);
            // error correction
            v -= lineNormal * (QVector2D::dotProduct(v, lineNormal) - linePos);
            return true;
        } else {
            return false;
        }
    }

    template <class Colorizer>
    void renderLandform(Colorizer&& colorizer)
    {

        struct RayDirectionMask
        {
            unsigned char mask;
            RayDirectionMask(const QVector2D &r)
            : mask(0){
                if (r.x() > 0.f) mask |= 1;
                if (r.x() < 0.f) mask |= 2;
                if (r.y() > 0.f) mask |= 4;
                if (r.y() < 0.f) mask |= 8;
            }
            bool positiveX() const { return mask & 1; }
            bool positiveY() const { return mask & 4; }
            bool negativeX() const { return mask & 2; }
            bool negativeY() const { return mask & 8; }
            bool anyX() const { return mask & 3; }
            bool anyY() const { return mask & 12; }
        };

        quint32 *colorOutput = transposedImage_.data();
        float *depthOutput = depthImage_.data();

        enum class Wall
        {
            PositiveX,
            NegativeX,
            PositiveY,
            NegativeY
        };

        const int imgWidth = image_->size().width();

        QtConcurrent::blockingMap(RangeIterator(0), RangeIterator((imgWidth + 63) >> 6), [=](int block) {

            const int imgWidth = image_->size().width();
            const int imgHeight = image_->size().height();
            const QVector3D sideIncl = rightVector * sceneDef.viewWidth / imgWidth;
            const QVector2D rayDir = cameraDir2D;

            RayDirectionMask rayDirMask(rayDir);

            const float *landform_ = terrain_->landformData();
            const quint32 *color_ = terrain_->colorData();
            int tWidth = terrain_->size().width();
            int tHeight = terrain_->size().height();

            Q_ASSUME(tWidth == 512);
            Q_ASSUME(tHeight == 512);
            tWidth = tHeight = 512;

            int minX = block << 6;
            int maxX = std::min(minX + 64, imgWidth);
            for (int x = minX; x < maxX; ++x) {
                quint32 *lineColorOutput = colorOutput + x * imgHeight;
                float *lineDepthOutput = depthOutput + x * imgHeight;

                const float *landform = landform_;
                const quint32 *color = color_;

                const QVector3D rayStart = sceneDef.eye + sideIncl * (x - (imgWidth >> 1));
                QVector2D scanStart(rayStart.x(), rayStart.y());
                bool first = true;
                Wall wall = Wall::PositiveX;
                if (rayDirMask.negativeX()) {
                    if (pushToBoundary(scanStart, rayDir, QVector2D(-1, 0), -tWidth, first))
                        wall = Wall::PositiveX;
                } else if (rayDirMask.positiveX()) {
                    if (pushToBoundary(scanStart, rayDir, QVector2D(1, 0), 0, first))
                        wall = Wall::NegativeX;
                }
                if (rayDirMask.negativeY()) {
                    if (pushToBoundary(scanStart, rayDir, QVector2D(0, -1), -tHeight, first))
                        wall = Wall::PositiveY;
                } else if (rayDirMask.positiveY()) {
                    if (pushToBoundary(scanStart, rayDir, QVector2D(0, 1), 0, first))
                        wall = Wall::NegativeY;
                }
                std::memset(lineColorOutput, 0xf, imgHeight * 4);
                std::fill(lineDepthOutput, lineDepthOutput + imgHeight, 1.e+4f);

                if (scanStart.x() < 0.f || scanStart.y() < 0.f ||
                    scanStart.x() > tWidth || scanStart.y() > tHeight) {
                    // Ray doesn't enter the terrain area
                    continue;
                }

                // Compute local Z -> screen Y transformation
                float zScale = static_cast<float>(imgHeight) / sceneDef.viewHeight *
                        upVector.z();
                float zOffset = static_cast<float>(imgHeight * 0.5f) - sceneDef.eye.z() * zScale;
                float zOffsetPerTravel = static_cast<float>(imgHeight) / sceneDef.viewHeight *
                        -std::sqrt(1.f - upVector.z() * upVector.z());

                QVector2D rayPos = scanStart;
                std::int_fast16_t rayCellX = static_cast<std::int_fast16_t>(std::floor(rayPos.x()));
                std::int_fast16_t rayCellY = static_cast<std::int_fast16_t>(std::floor(rayPos.y()));
                std::int_fast16_t nextRayCellX = rayCellX;
                std::int_fast16_t nextRayCellY = rayCellY;
                if (rayDirMask.positiveX()) ++nextRayCellX;
                if (rayDirMask.positiveY()) ++nextRayCellY;

                switch (wall) {
                case Wall::PositiveX:
                    //--rayCellX; --nextRayCellX;
                    break;
                case Wall::NegativeX:
                    --rayCellX; --nextRayCellX;
                    break;
                case Wall::PositiveY:
                    //--rayCellY; --nextRayCellY;
                    break;
                case Wall::NegativeY:
                    --rayCellY; --nextRayCellY;
                    break;
                }

                const float invRayDirX = 1.f / rayDir.x();
                const float invRayDirY = 1.f / rayDir.y();

                zOffset += QVector2D::dotProduct((scanStart - QVector2D(sceneDef.eye.x(), sceneDef.eye.y())), rayDir)
                        * zOffsetPerTravel;

                float lastImageFY = zOffset + zScale * 64.f;
                std::int_fast16_t lastImageY = static_cast<std::int_fast16_t>
                        (std::min<float>(lastImageFY, imgHeight));
                quint32 lastColor = 0x7f0000;

                float travelDistance = 0.f;
                float travelDistanceDelta = -1.f / zOffsetPerTravel;

                auto fillWall = [&](std::int_fast16_t y) {
                    y = std::max<std::int_fast16_t>(y, 0);
                    while(lastImageY > y) {
                        --lastImageY;
                        lineColorOutput[lastImageY] = lastColor;
                        lineDepthOutput[lastImageY] = travelDistance;
                    }
                };
                auto fillFloor = [&](std::int_fast16_t y) {
                    y = std::max<std::int_fast16_t>(y, 0);
                    if (lastImageY > y) {
                        float depth = travelDistance;
                        depth += (lastImageFY - (lastImageY + 1)) * travelDistanceDelta;
                        do {
                            --lastImageY;
                            lineColorOutput[lastImageY] = lastColor;
                            lineDepthOutput[lastImageY] = depth;
                            depth += travelDistanceDelta;
                        } while (lastImageY > y);
                    }
                };

                int prefetchIndex = (rayDirMask.positiveY() ? 1 : -1) * tWidth;

                char enteredScreen = 0;

                landform += rayCellX + rayCellY * tWidth;
                color += rayCellX + rayCellY * tWidth;

                while (true) {
                    float xHitTime = 114514.f;
                    float yHitTime;

                    {
                        _mm_prefetch(reinterpret_cast<char const *>(landform + prefetchIndex), _MM_HINT_T0);
                        _mm_prefetch(reinterpret_cast<char const *>(color + prefetchIndex), _MM_HINT_T0);
                    }

                    if (rayDirMask.anyX()) {
                        xHitTime = (static_cast<float>(nextRayCellX) - rayPos.x()) * invRayDirX;
                    }
                    if (rayDirMask.anyY() &&
                        (yHitTime = (static_cast<float>(nextRayCellY) - rayPos.y()) * invRayDirY) < xHitTime) {
                        // Hit Y wall
                        float offs = yHitTime * zOffsetPerTravel;
                        zOffset += offs;

                        rayPos.setX(rayPos.x() + rayDir.x() * yHitTime);
                        rayPos.setY(nextRayCellY);

                        // floor
                        float nextImgFY = lastImageFY + offs;
                        auto nextImgY = static_cast<std::int_fast16_t>(nextImgFY);
                        fillFloor(nextImgY);
                        lastImageFY = nextImgFY;

                        // move to next cell
                        if (rayDirMask.positiveY()) {
                            ++rayCellY; ++nextRayCellY;
                            if (rayCellY >= tHeight) {
                                break;
                            }
                            landform += tWidth; color += tWidth;
                        } else {
                            --rayCellY; --nextRayCellY;
                            if (rayCellY < 0) {
                                break;
                            }
                            landform -= tWidth; color -= tWidth;
                        }
                        travelDistance += yHitTime;
                    } else {
                        // Hit X wall
                        float offs = xHitTime * zOffsetPerTravel;
                        zOffset += offs;

                        rayPos.setY(rayPos.y() + rayDir.y() * xHitTime);
                        rayPos.setX(nextRayCellX);

                        // floor
                        float nextImgFY = lastImageFY + offs;
                        auto nextImgY = static_cast<std::int_fast16_t>(nextImgFY);
                        fillFloor(nextImgY);
                        lastImageFY = nextImgFY;

                        // move to next cell
                        if (rayDirMask.positiveX()) {
                            ++rayCellX; ++nextRayCellX;
                            if (rayCellX >= tWidth) {
                                break;
                            }
                            ++landform; ++color;
                        } else {
                            --rayCellX; --nextRayCellX;
                            if (rayCellX < 0) {
                                break;
                            }
                            --landform; --color;
                        }
                        travelDistance += xHitTime;
                    }

                    auto darken = [](quint32 col) -> quint32 {
                        auto c = _mm_setr_epi32(col, 0, 0, 0);
                        auto c2 = _mm_avg_epu8(c, _mm_setzero_si128());
                        c2 = _mm_avg_epu8(c, c2);
                        return _mm_extract_epi32(c2, 0);
                    };

                    if (enteredScreen || zOffset < imgHeight) { // map fragment visible on screen?
                        // sample
                        float land = *landform;
                        quint32 col = *color;
                        col = colorizer(col, land);

                        // draw wall
                        lastColor = darken(col);
                        float nextImgFY = land * zScale + zOffset;
                        auto nextImgY = static_cast<std::int_fast16_t>(nextImgFY);
                        fillWall(nextImgY);

                        // prepare for next floor drawing
                        lastColor = col;
                        lastImageFY = nextImgFY;

                        enteredScreen = 1;
                    }

                    if (lastImageY <= 0) {
                        // screen is filled; early out
                        break;
                    }
                }

                // fix the depth
                float fixAmt = QVector2D::dotProduct(rayDir, scanStart) + zBias;

                fixAmt -= 1.e-1f; // intentionally add bias to prevent flickering

                auto fixAmt4 = _mm_set1_ps(fixAmt);
                for (int y = 0; y < imgHeight; y += 4) {
                    auto m = _mm_loadu_ps(lineDepthOutput + y);
                    m = _mm_add_ps(m, fixAmt4);
                    _mm_storeu_ps(lineDepthOutput + y, m);
                }
            }
        });
    }

    void applySharpeningFilter()
    {
        const int imgWidth = image_->size().width();
        const int imgHeight = image_->size().height();
        quint32 *colorOutput = transposedImage_.data();
        float *depthInput = depthImage_.data();

        Q_ASSUME(imgHeight >= 8);

        QtConcurrent::blockingMap(RangeIterator(0), RangeIterator((imgWidth + 63) >> 6), [=](int block) {
            int minX = block << 6;
            int maxX = std::min(minX + 64, imgWidth);
            for (int x = minX; x < maxX; ++x) {
                quint32 *lineColorOutput = colorOutput + x * imgHeight;
                float *lineDepthInput1 = depthInput + x * imgHeight;
                float *lineDepthInput2 = depthInput + (x == 0 ? x : x - 1) * imgHeight;
                float *lineDepthInput3 = depthInput + (x + 1 == imgWidth ? x : x + 1) * imgHeight;

                auto processSpan = [&](quint32 *color, __m128 depth1, float *depth2, float *depth3,
                        __m128 depth4, __m128 depth5) {
                    auto t = depth1;
                    t = _mm_mul_ps(t, _mm_set1_ps(4.f));
                    t = _mm_sub_ps(t, _mm_loadu_ps(depth2));
                    t = _mm_sub_ps(t, _mm_loadu_ps(depth3));
                    t = _mm_sub_ps(t, depth4);
                    t = _mm_sub_ps(t, depth5);

                    t = _mm_max_ps(t, _mm_set1_ps(-.5f));
                    t = _mm_min_ps(t, _mm_set1_ps(1.5f));

                    // calculate color coef
                    auto coeff = _mm_mul_ps(t, _mm_set1_ps(-16.f));   // f32 * 4
                    coeff = _mm_add_ps(coeff, _mm_set1_ps(64.5f));   // f32 * 4
                    coeff = _mm_cvttps_epi32(coeff);                // i32 * 4
                    coeff = _mm_packs_epi32(coeff, coeff);          // i16 * 4 + pad
                    coeff = _mm_unpacklo_epi16(coeff, coeff);       // i16 [c1, c1, c2, c2, c3, c3, c4, c4]

                    // load color
                    auto col = _mm_loadu_si128(reinterpret_cast<__m128i*>(color));
                    auto colLo = _mm_unpacklo_epi8(col, _mm_setzero_si128());
                    colLo = _mm_srai_epi16(_mm_mullo_epi16(colLo, _mm_unpacklo_epi16(coeff, coeff)), 6);
                    auto colHi = _mm_unpackhi_epi8(col, _mm_setzero_si128());
                    colHi = _mm_srai_epi16(_mm_mullo_epi16(colHi, _mm_unpackhi_epi16(coeff, coeff)), 6);
                    col = _mm_packus_epi16(colLo, colHi);

                    _mm_storeu_si128(reinterpret_cast<__m128i*>(color), col);
                };

                {
                    auto depth = _mm_loadu_ps(lineDepthInput1);
                    processSpan(lineColorOutput, depth,
                                lineDepthInput2,
                                lineDepthInput3,
                                _mm_shuffle_ps(depth, depth, 0x90),
                                _mm_loadu_ps(lineDepthInput1 + 1));
                }
                {
                    auto depth = _mm_loadu_ps(lineDepthInput1 + imgHeight - 4);
                    processSpan(lineColorOutput + imgHeight - 4, depth,
                                lineDepthInput2 + imgHeight - 4,
                                lineDepthInput3 + imgHeight - 4,
                                _mm_loadu_ps(lineDepthInput1 + imgHeight - 5),
                                _mm_shuffle_ps(depth, depth, 0xf9));
                }
                lineColorOutput += 4;
                lineDepthInput1 += 4;
                lineDepthInput2 += 4;
                lineDepthInput3 += 4;
                for (int y = 4; y < imgHeight - 4; y += 4) {
                    processSpan(lineColorOutput,
                                _mm_loadu_ps(lineDepthInput1),
                                lineDepthInput2, lineDepthInput3,
                                _mm_loadu_ps(lineDepthInput1 - 1),
                                _mm_loadu_ps(lineDepthInput1 + 1));
                    lineColorOutput += 4;
                    lineDepthInput1 += 4;
                    lineDepthInput2 += 4;
                    lineDepthInput3 += 4;
                }
            }
        });
    }

    class DrawingContext : public TerrainViewDrawingContext
    {
        TerrainViewPrivate &p;

        template <class Blender>
        void projectBitmapInner(QImage &decal, const QPointF &origin, const QSizeF &size, const Blender& blender) {
            const int imgWidth =  p.image_->size().width();
            const int imgHeight = p.image_->size().height();
            const int decalWidth =  decal.size().width();
            const int decalHeight = decal.size().height();
            quint32 *colorInput = p.transposedImage_.data();
            float *depthInput = p.depthImage_.data();
            const QVector3D sideIncl = p.rightVector * p.sceneDef.viewWidth / imgWidth;
            const QVector2D rayDir = p.cameraDir2D;

            const quint32 *decalPixels = reinterpret_cast<quint32 *>(decal.bits());

            __m128 rayDirMM = _mm_setr_ps(rayDir.x(), rayDir.y(), 0, 0);
            rayDirMM = _mm_shuffle_ps(rayDirMM, rayDirMM, 0x44);
            __m128 decalOrigin = _mm_setr_ps(-origin.x(), -origin.y(), 0, 0);
            decalOrigin = _mm_shuffle_ps(decalOrigin, decalOrigin, 0x44);
            __m128 decalScaler = _mm_setr_ps(decalWidth / size.width(), decalHeight / size.height(), 0, 0);
            decalScaler = _mm_shuffle_ps(decalScaler, decalScaler, 0x44);

            __m128 decalSize = _mm_setr_epi32(decalWidth, decalHeight, 0, 0);
            decalSize = _mm_shuffle_epi32(decalSize, 0x44);

            QtConcurrent::blockingMap(RangeIterator(0), RangeIterator((imgWidth + 63) >> 6), [=](int block) {
                int minX = block << 6;
                int maxX = std::min(minX + 64, imgWidth);
                for (int x = minX; x < maxX; ++x) {
                    quint32 *lineColorInput = colorInput + x * imgHeight;
                    float *lineDepthInput = depthInput + x * imgHeight;

                    const QVector3D rayStart = p.sceneDef.eye + sideIncl * (x - (imgWidth >> 1));
                    QVector2D scanStart(rayStart.x(), rayStart.y());
                    scanStart += rayDir * -(QVector2D::dotProduct(scanStart, rayDir) + p.zBias);
                    // depth 0 = scanStart

                    __m128 depthOrigin = _mm_setr_ps(scanStart.x(), scanStart.y(), scanStart.x(), scanStart.y());

                    auto doBlend = [&](int x, int y, quint32 *color) {
                        // sample image
                        auto srcColor = _mm_castps_si128(_mm_load_ss(
                            reinterpret_cast<const float*>(decalPixels + x + y * decalWidth)));
                        auto destColor = _mm_castps_si128(_mm_load_ss(
                            reinterpret_cast<float*>(color)));
                        auto outColor = blender.blend(srcColor, destColor);
                        _mm_store_ss(reinterpret_cast<float*>(color), _mm_castsi128_ps(outColor));
                    };

                    for (int y = 0; y < imgHeight; y += 4) {
                        auto depth = _mm_loadu_ps(lineDepthInput + y);
                        auto coords1 = _mm_add_ps(depthOrigin, _mm_mul_ps(_mm_shuffle_ps(depth, depth, 0x50), rayDirMM));
                        auto coords2 = _mm_add_ps(depthOrigin, _mm_mul_ps(_mm_shuffle_ps(depth, depth, 0xfa), rayDirMM));

                        // transform to image coordinate
                        coords1 = _mm_mul_ps(_mm_add_ps(coords1, decalOrigin), decalScaler);
                        coords2 = _mm_mul_ps(_mm_add_ps(coords2, decalOrigin), decalScaler);

                        coords1 = _mm_floor_ps(coords1);
                        coords2 = _mm_floor_ps(coords2);

                        auto iCoords1 = _mm_cvttps_epi32(coords1);
                        auto iCoords2 = _mm_cvttps_epi32(coords2);

                        // inside image?
                        auto inside1 = _mm_cmplt_epi32(_mm_srli_epi32(_mm_slli_epi32(iCoords1, 1), 1), decalSize);
                        auto inside2 = _mm_cmplt_epi32(_mm_srli_epi32(_mm_slli_epi32(iCoords2, 1), 1), decalSize);
                        // iCoords1 = _mm_andnot_si128(inside1, iCoords1);
                        // iCoords2 = _mm_andnot_si128(inside2, iCoords2);
                        auto inside = _mm_packs_epi32(inside1, inside2); // [xInside[16], yInside[16]] x 4
                        inside = _mm_and_ps(inside, _mm_srli_epi32(inside, 16)); // [inside[16], invalid[16]] * 4
                        auto insideMask = _mm_movemask_epi8(inside); // [inside[2], invalid[2]] * 4
                        if (insideMask & 0x1111) {
                            if (insideMask & 0x1) {
                                doBlend(_mm_extract_epi16(iCoords1, 0), _mm_extract_epi16(iCoords1, 2),
                                        lineColorInput + y);
                            }
                            if (insideMask & 0x10) {
                                doBlend(_mm_extract_epi16(iCoords1, 4), _mm_extract_epi16(iCoords1, 6),
                                        lineColorInput + y + 1);
                            }
                            if (insideMask & 0x100) {
                                doBlend(_mm_extract_epi16(iCoords2, 0), _mm_extract_epi16(iCoords2, 2),
                                        lineColorInput + y + 2);
                            }
                            if (insideMask & 0x1000) {
                                doBlend(_mm_extract_epi16(iCoords2, 4), _mm_extract_epi16(iCoords2, 6),
                                        lineColorInput + y + 3);
                            }
                        }
                    }

                }
            });
        }
    public:
        DrawingContext(TerrainViewPrivate &p) : p(p) {}

        void projectBitmap(QImage &img, const QPointF &origin, const QSizeF &size, bool additive) override {
            if (img.format() != QImage::Format_RGB32){
                Q_UNREACHABLE();
            }
            struct NormalBlender {
                inline __m128i blend(__m128i src, __m128i) const {
                    return src;
                }
            };
            struct AdditiveBlender {
                inline __m128i blend(__m128i src, __m128i dest) const {
                    return _mm_adds_epu8(src, dest);
                }
            };
            if (additive) {
                projectBitmapInner(img, origin, size, AdditiveBlender());
            } else {
                projectBitmapInner(img, origin, size, NormalBlender());
            }
        }
    };

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

    QPointF rayCastByDepthBuffer(const QPoint &pt)
    {
        const int imgWidth = image_->size().width();
        const int imgHeight = image_->size().height();
        Q_ASSERT(pt.x() >= 0); Q_ASSERT(pt.y() >= 0);
        Q_ASSERT(pt.x() < imgWidth); Q_ASSERT(pt.y() < imgHeight);

        const QVector3D sideIncl = rightVector * sceneDef.viewWidth / imgWidth;
        const QVector2D rayDir = cameraDir2D;

        float depth = depthImage_[pt.x() * imgHeight + pt.y()];
        const QVector3D rayStart = sceneDef.eye + sideIncl * (pt.x() - (imgWidth >> 1));
        QVector2D scanPos(rayStart.x(), rayStart.y());
        scanPos += rayDir * (depth - zBias - QVector2D::dotProduct(scanPos, rayDir));

        return QPointF(scanPos.x(), scanPos.y());
    }

    QImage &render(QSize size, const SceneDefinition &def)
    {
        Q_Q(TerrainView);
        if (size.isEmpty()) {
            size = QSize(1, 1);
        }
        size = QSize((size.width() + 3) & ~3, (size.height() + 3) & ~3);
        if (!image_ || image_->size() != size) {
            // Regenerate image
            image_.reset(new QImage(size, QImage::Format_RGB32));
            depthImage_.resize(size.width() * size.height());
            transposedImage_.resize(size.width() * size.height());
        }

        sceneDef = def;
        sceneDef.cameraDir.normalize();

        zBias = terrain_->size().width() + terrain_->size().height();

        QVector3D up(0, 0, -1);
        rightVector = QVector3D::crossProduct(sceneDef.cameraDir, up).normalized();
        upVector = QVector3D::crossProduct(sceneDef.cameraDir, rightVector);

        cameraDir2D = QVector2D(sceneDef.cameraDir.x(), sceneDef.cameraDir.y()).normalized();

        if (viewOptions.colorizeAltitude)
            renderLandform([](quint32 color, float alt) {
                (void) color;
                if (alt >= 62.5f) {
                    return 0x102040;
                }
                auto m = _mm_set1_ps(alt);
                auto k = _mm_setr_ps(63.f, 43.f, 23.f, 3.f);
                m = _mm_max_ps(_mm_sub_ps(m, k), _mm_sub_ps(k, m));
                m = _mm_sub_ps(_mm_set1_ps(20.f), m);
                //m = _mm_max_ps(_mm_set1_ps(0.f), m);
                m = _mm_mul_ps(m, _mm_set1_ps(255.f / 10.f));

                auto mI = _mm_cvttps_epi32(m);
                mI = _mm_packs_epi32(mI, _mm_setzero_si128());
                mI = _mm_packus_epi16(mI, mI);
                mI = _mm_or_si128(mI, _mm_srli_epi64(mI, 24));

                return _mm_extract_epi32(mI, 0);
            });
        else
            renderLandform([](quint32 color, float alt) { return color; });
        if (viewOptions.showEdges)
            applySharpeningFilter();

        DrawingContext dc(*this);
        emit q->terrainPaint(&dc);

        transposeImage(reinterpret_cast<quint32 *>(image_->bits()), size, transposedImage_.data());

        return *image_;
    }
};

TerrainView::TerrainView(QWidget *parent) :
    QWidget(parent),
    d_ptr(new TerrainViewPrivate(this))
{
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
}

TerrainView::~TerrainView()
{
    delete d_ptr;
}

QPoint TerrainView::rayCast(const QPoint &p)
{
    Q_D(TerrainView);
    if (p.x() <= 0 || p.y() <= 0 || p.x() >= d->image_->size().width() ||
            p.y() >= d->image_->size().height()) {
        return QPoint(-1000, -1000);
    }
    auto pt = d->rayCastByDepthBuffer(p);
    return QPoint(std::floor(pt.x()), std::floor(pt.y()));
}

void TerrainView::setTerrain(QSharedPointer<Terrain> terrain)
{
    Q_D(TerrainView);
    if (d->terrain_ == terrain) {
        return;
    }
    d->terrain_ = terrain;
    update();
}

QSharedPointer<Terrain> TerrainView::terrain()
{
    Q_D(TerrainView);
    return d->terrain_;
}

void TerrainView::setViewOptions(const TerrainViewOptions &opt)
{
    Q_D(TerrainView);
    d->viewOptions = opt;
    update();
}

const TerrainViewOptions &TerrainView::viewOptions()
{
    Q_D(TerrainView);
    return d->viewOptions;
}

void TerrainView::paintEvent(QPaintEvent *e)
{
    Q_D(TerrainView);
    auto size = this->size();


    SceneDefinition def;
    def.cameraDir = QVector3D(std::cos(d->yaw_) * std::cos(d->pitch_),
                              std::sin(d->yaw_) * std::cos(d->pitch_),
                              std::sin(d->pitch_));
    def.eye = d->centerPos_ - def.cameraDir;
    def.viewWidth = size.width() / d->scale_;
    def.viewHeight = size.height() / d->scale_;

    auto img = d->render(size, def);

    QPainter painter(this);
    painter.drawImage(0, 0, img);

    emit clientPaint(e);
}

void TerrainView::mousePressEvent(QMouseEvent *e)
{
    Q_D(TerrainView);

    if (e->button() != Qt::RightButton) {
        e->setAccepted(false);
        emit clientMousePressed(e);
        if (e->isAccepted()) {
            return;
        }
    }

    d->dragging_ = true;
    d->dragPos_ = e->pos();
    d->dragStartYaw_ = d->yaw_;
    d->dragStartPitch_ = d->pitch_;
    d->dragStartCenterPos_ = d->centerPos_;
    d->dragShiftMode_ = ((e->modifiers() & Qt::ShiftModifier) != 0 == ((e->buttons() & Qt::RightButton) != 0));


    e->accept();
}

void TerrainView::mouseReleaseEvent(QMouseEvent *e)
{
    Q_D(TerrainView);

    if (d->dragging_) {
        d->dragging_ = false;
        e->accept();
    } else {
        emit clientMouseReleased(e);
    }
}

void TerrainView::mouseMoveEvent(QMouseEvent *e)
{
    Q_D(TerrainView);
    if (d->dragging_) {
        bool shiftMode = ((e->modifiers() & Qt::ShiftModifier) != 0 == ((e->buttons() & Qt::RightButton) != 0));
        if (shiftMode != d->dragShiftMode_) {
            d->dragStartYaw_ = d->yaw_;
            d->dragStartPitch_ = d->pitch_;
            d->dragStartCenterPos_ = d->centerPos_;
            d->dragShiftMode_ = shiftMode;
            d->dragPos_ = e->pos();
        }
        float dx = e->pos().x() - d->dragPos_.x();
        float dy = e->pos().y() - d->dragPos_.y();
        QPoint diff = e->pos() - d->dragPos_;

        if (shiftMode) {
            QVector3D ddx = d->rightVector;
            QVector3D ddy = d->upVector;
            auto newPos = d->dragStartCenterPos_;
            newPos -= QVector2D(ddx.x(), ddx.y()) * (dx * d->sceneDef.viewWidth / d->image_->size().width());
            newPos -= QVector2D(ddy.x(), ddy.y()) * (dy / (1.f - ddy.z() * ddy.z())
                                                      * d->sceneDef.viewHeight / d->image_->size().height());
            newPos.setX(std::max(newPos.x(), 0.f));
            newPos.setY(std::max(newPos.y(), 0.f));
            newPos.setX(std::min<float>(newPos.x(), d->terrain_->size().width()));
            newPos.setY(std::min<float>(newPos.y(), d->terrain_->size().height()));
            d->centerPos_ = newPos;
        } else {
            d->yaw_ = d->dragStartYaw_ + diff.x() * 0.01f;
            d->pitch_ = std::max<float>(std::min<float>(d->dragStartPitch_ + diff.y() * 0.01f, M_PI * 0.499f), 0.1f);
        }

        update();
        e->accept();
    } else {
        emit clientMouseMoved(e);
    }
}

void TerrainView::wheelEvent(QWheelEvent *e)
{
    Q_D(TerrainView);

    float &scale = d->scale_;
    scale *= std::exp2f(e->angleDelta().y() * 0.004f);
    scale = std::min(scale, 100.f);
    scale = std::max(scale, .1f);

    update();
    e->accept();
}

void TerrainView::showOptionsWindow()
{
    if (!optionsWindow) {
        optionsWindow.reset(new TerrainViewOptionsWindow(this, this));
        connect(optionsWindow.data(), SIGNAL(destroyed()), SLOT(optionsWindowClosed()));
    }
    optionsWindow->show();
}

void TerrainView::optionsWindowClosed()
{
    if (optionsWindow)
        optionsWindow.take()->deleteLater();
}
