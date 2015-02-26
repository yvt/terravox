#include "terrainview_p.h"
#include <QtConcurrent>
#include <cmath>
#include <smmintrin.h>
#include "terrain.h"

#if defined(__APPLE__)
#  define memset std::memset
#endif

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
void TerrainView::TerrainViewPrivate::renderLandform1(Colorizer&& colorizer)
{
    if (viewOptions.ambientOcclusion)
        renderLandform<Colorizer, true>(std::move(colorizer));
    else
        renderLandform<Colorizer, false>(std::move(colorizer));
}

template <class Colorizer, bool WriteHeight>
void TerrainView::TerrainViewPrivate::renderLandform(Colorizer&& colorizer)
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
    float *heightOutput = WriteHeight ? heightImage_.data() : nullptr;

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
            float *lineHeightOutput = WriteHeight ? heightOutput + x * imgHeight : nullptr;

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
            memset(lineColorOutput, 0xf, imgHeight * 4);
            std::fill(lineDepthOutput, lineDepthOutput + imgHeight, 1.e+4f);
            if (WriteHeight)
                std::fill(lineHeightOutput, lineHeightOutput + imgHeight, 1.e+4f);

            // draw background
            // FIXME: reduce overdraw
            {
                QVector3D bgScanStart(rayStart.x(), rayStart.y(), sceneDef.eye.z());
                float floorLevel = 64.f;
                bgScanStart += sceneDef.cameraDir * ((floorLevel - bgScanStart.z()) / sceneDef.cameraDir.z());
                QVector3D bgScanDelta = QVector3D(rayDir.x(), rayDir.y(), 0) *
                        -(sceneDef.viewHeight / imgHeight / sceneDef.cameraDir.z());
                bgScanStart -= bgScanDelta * (imgHeight >> 1);
                bgScanStart *= .5f; bgScanDelta *= .5f;
                bgScanStart += QVector3D(0.25f, 0.25f, 0);

                // compute coefs for antialiasing
                float contrastX = 1.f / std::abs(bgScanDelta.x());
                float contrastY = 1.f / std::abs(bgScanDelta.y());
                float offsetX = contrastX * -.25f + .5f;
                float offsetY = contrastY * -.25f + .5f;

                auto contrastX4 = _mm_set1_ps(contrastX);
                auto contrastY4 = _mm_set1_ps(contrastY);
                auto offsetX4 = _mm_set1_ps(offsetX);
                auto offsetY4 = _mm_set1_ps(offsetY);
                auto bgScanPosX = _mm_setr_ps(
                            bgScanStart.x(),
                            bgScanStart.x() + bgScanDelta.x(),
                            bgScanStart.x() + bgScanDelta.x() * 2,
                            bgScanStart.x() + bgScanDelta.x() * 3);
                auto bgScanPosY = _mm_setr_ps(
                            bgScanStart.y(),
                            bgScanStart.y() + bgScanDelta.y(),
                            bgScanStart.y() + bgScanDelta.y() * 2,
                            bgScanStart.y() + bgScanDelta.y() * 3);
                auto bgScanDeltaX = _mm_set1_ps(bgScanDelta.x() * 4);
                auto bgScanDeltaY = _mm_set1_ps(bgScanDelta.y() * 4);
                auto mm_abs_ps = [](__m128 m) {
                    return _mm_castsi128_ps(_mm_srli_epi32(_mm_slli_epi32(_mm_castps_si128(m), 1), 1));
                };

                for (int y = 0; y < imgHeight; y += 4) {
                    auto fracX = _mm_sub_ps(bgScanPosX, _mm_floor_ps(bgScanPosX));
                    auto fracY = _mm_sub_ps(bgScanPosY, _mm_floor_ps(bgScanPosY));

                    fracX = mm_abs_ps(_mm_sub_ps(fracX, _mm_set1_ps(0.5f)));
                    fracY = mm_abs_ps(_mm_sub_ps(fracY, _mm_set1_ps(0.5f)));

                    fracX = _mm_add_ps(offsetX4, _mm_mul_ps(fracX, contrastX4));
                    fracY = _mm_add_ps(offsetY4, _mm_mul_ps(fracY, contrastY4));

                    fracX = _mm_max_ps(fracX, _mm_set1_ps(0.f));
                    fracY = _mm_max_ps(fracY, _mm_set1_ps(0.f));
                    fracX = _mm_min_ps(fracX, _mm_set1_ps(1.f));
                    fracY = _mm_min_ps(fracY, _mm_set1_ps(1.f));

                    auto pat1 = _mm_mul_ps(fracX, fracY);
                    auto pat2 = _mm_sub_ps(_mm_set1_ps(1.f), fracX);
                    auto pat3 = _mm_sub_ps(_mm_set1_ps(1.f), fracY);
                    auto pat = _mm_add_ps(pat1, _mm_mul_ps(pat2, pat3));


                    // generate color
                    pat = _mm_mul_ps(pat, _mm_set1_ps(504.f));
                    pat = _mm_add_ps(_mm_set1_ps(500.f), pat);

                    // gamma correction
                    pat = _mm_mul_ps(pat, _mm_rsqrt_ps(pat));

                    auto patI = _mm_cvtps_epi32(pat);
                    patI = _mm_or_si128(patI, _mm_slli_epi32(patI, 16));
                    patI = _mm_or_si128(patI, _mm_slli_epi32(patI, 8));
                    _mm_storeu_si128(reinterpret_cast<__m128i*>(lineColorOutput + y), patI);

                    bgScanPosX = _mm_add_ps(bgScanPosX, bgScanDeltaX);
                    bgScanPosY = _mm_add_ps(bgScanPosY, bgScanDeltaY);
                }

            }

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

            float lastHeight = 64.f;
            float lastImageFY = zOffset + zScale * lastHeight;
            std::int_fast16_t lastImageY = static_cast<std::int_fast16_t>
                    (std::min<float>(lastImageFY, imgHeight));
            quint32 lastColor = 0x7f0000;
            auto startImageY = lastImageY;

            float travelDistance = 0.f;
            float travelDistanceDelta = -1.f / zOffsetPerTravel;

            auto fillWall = [&](std::int_fast16_t y) {
                y = std::max<std::int_fast16_t>(y, 0);
                while(lastImageY > y) {
                    --lastImageY;
                    lineColorOutput[lastImageY] = lastColor;
                    lineDepthOutput[lastImageY] = travelDistance;
                    if (WriteHeight) lineHeightOutput[lastImageY] = lastHeight;
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
                        if (WriteHeight) lineHeightOutput[lastImageY] = lastHeight;
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
                    lastHeight = land;

                    enteredScreen = 1;
                }

                if (lastImageY <= 0) {
                    // screen is filled; early out
                    break;
                }
            }

            // fix the depth
            float fixAmt = QVector2D::dotProduct(rayDir, scanStart) + zBias;

            fixAmt += depthBufferBias; // intentionally add bias to prevent flickering

            auto fixAmt4 = _mm_set1_ps(fixAmt);
            {
                int y = lastImageY;
                int endY = startImageY;
                while (y < endY && (y & 3)) {
                    lineDepthOutput[y] += fixAmt; ++y;
                }
                int endYAligned = endY - 3;
                while (y < endYAligned) {
                    auto m = _mm_loadu_ps(lineDepthOutput + y);
                    m = _mm_add_ps(m, fixAmt4);
                    _mm_storeu_ps(lineDepthOutput + y, m);
                    y += 4;
                }
                while (y < endY) {
                    lineDepthOutput[y] += fixAmt; ++y;
                }
            }
        }
    });
}

void TerrainView::TerrainViewPrivate::applySharpeningFilter()
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


QPointF TerrainView::TerrainViewPrivate::rayCastByDepthBuffer(const QPoint &pt)
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

QImage &TerrainView::TerrainViewPrivate::render(QSize size, const SceneDefinition &def)
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
        heightImage_.resize(size.width() * size.height());
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
        renderLandform1([](quint32 color, float alt) {
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
        renderLandform1([](quint32 color, float alt) { return color; });
    if (viewOptions.showEdges)
        applySharpeningFilter();
    if (viewOptions.ambientOcclusion)
        applyAmbientOcclusion();

    emitTerrainPaint();

    transposeImage(reinterpret_cast<quint32 *>(image_->bits()), size, transposedImage_.data());

    return *image_;
}

void TerrainView::TerrainViewPrivate::terrainUpdate(const QRect &bounds)
{
    if (!aoMapDirtyRect_.isNull())
        aoMapDirtyRect_ = aoMapDirtyRect_.united(bounds);
    else
        aoMapDirtyRect_ = bounds;
}
