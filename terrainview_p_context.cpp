#include "terrainview_p.h"
#include <smmintrin.h>
#include <QtConcurrent>
#include "cpu.h"

class TerrainView::TerrainViewPrivate::DrawingContext : public TerrainViewDrawingContext
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

        SseRoundingModeScope roundingModeScope(_MM_ROUND_DOWN);
        (void) roundingModeScope;

        const quint32 *decalPixels = reinterpret_cast<quint32 *>(decal.bits());

        __m128 rayDirMM = _mm_setr_ps(rayDir.x(), rayDir.y(), 0, 0);
        rayDirMM = _mm_shuffle_ps(rayDirMM, rayDirMM, 0x44);
        __m128 decalOrigin = _mm_setr_ps(-origin.x(), -origin.y(), 0, 0);
        decalOrigin = _mm_shuffle_ps(decalOrigin, decalOrigin, 0x44);
        __m128 decalScaler = _mm_setr_ps(decalWidth / size.width(), decalHeight / size.height(), 0, 0);
        decalScaler = _mm_shuffle_ps(decalScaler, decalScaler, 0x44);

        __m128i decalSize = _mm_setr_epi32(decalWidth, decalHeight, 0, 0);
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

                    auto iCoords1 = _mm_cvttps_epi32(coords1);
                    auto iCoords2 = _mm_cvttps_epi32(coords2);

                    // inside image?
                    auto inside1 = _mm_cmplt_epi32(_mm_srli_epi32(_mm_slli_epi32(iCoords1, 1), 1), decalSize);
                    auto inside2 = _mm_cmplt_epi32(_mm_srli_epi32(_mm_slli_epi32(iCoords2, 1), 1), decalSize);
                    // iCoords1 = _mm_andnot_si128(inside1, iCoords1);
                    // iCoords2 = _mm_andnot_si128(inside2, iCoords2);
                    auto inside = _mm_packs_epi32(inside1, inside2); // [xInside[16], yInside[16]] x 4
                    inside = _mm_and_si128(inside, _mm_srli_epi32(inside, 16)); // [inside[16], invalid[16]] * 4
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

void TerrainView::TerrainViewPrivate::emitTerrainPaint()
{
    Q_Q(TerrainView);

    DrawingContext dc(*this);
    emit q->terrainPaint(&dc);
}

