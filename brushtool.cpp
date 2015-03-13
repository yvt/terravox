#include "brushtool.h"
#include "terrain.h"
#include <cmath>
#include "terrainedit.h"
#include <xmmintrin.h>
#include <smmintrin.h>
#include "dithersampler.h"
#include <numeric>
#include "temporarybuffer.h"
#include "coherentnoisegenerator.h"
#include <random>
#include "cpu.h"

#if defined(__APPLE__)
#  define sqrtf std::sqrtf
#endif

BrushTool::BrushTool(BrushType type, QObject *parent) :
    QObject(parent),
    type_(type)
{
    parameters_.size = 10;
    parameters_.tipType = BrushTipType::Sphere;
    switch (type) {
    case BrushType::Lower:
    case BrushType::Raise:
        parameters_.strength = 5.f;
        break;
    default:
        parameters_.strength = 50.f;
        break;
    }

    parameters_.pressureMode = BrushPressureMode::AirBrush;
    parameters_.color = 0;
    parameters_.scale = 10.f;
    parameters_.randomize();
}

void BrushToolParameters::randomize()
{
    std::random_device rgen;
    std::uniform_int_distribution<quint32> dist(0, std::numeric_limits<quint32>::max());
    seed = dist(rgen);
}

BrushTool::~BrushTool()
{

}

void BrushTool::setParameters(const BrushToolParameters &t)
{
    parameters_ = t;
    emit parameterChanged();
    discardTip();
}

static CoherentNoiseGenerator noiseGen(9, 1);
static uint noiseGenSeed = 1;

QSharedPointer<Terrain> BrushTool::tip(QPoint origin)
{
    bool needToGenerate = false;
    if (!tip_) {
        tip_ = QSharedPointer<Terrain>::create(QSize(parameters_.size, parameters_.size));
        needToGenerate = true;
    }
    if (origin.x() < -500) {
        origin = lastTipOrigin_;
    }
    if (origin != lastTipOrigin_) {
        switch (parameters_.tipType) {
        case BrushTipType::Mountains:
            needToGenerate = true;
            break;
        default:
            // position invariant
            break;
        }
    }
    if (needToGenerate) {
        Terrain *t = tip_.data();
        auto size = parameters_.size;

        float scale = 1.f / size;

        switch (parameters_.tipType) {
        case BrushTipType::Mountains:
            {
                // Set rounding mode (required by CoherentNoiseGenerator)
                SseRoundingModeScope roundingModeScope(_MM_ROUND_DOWN);
                (void) roundingModeScope;

                if (noiseGenSeed != parameters_.seed) {
                    noiseGenSeed = parameters_.seed;
                    noiseGen.randomize(static_cast<std::uint_fast32_t>(noiseGenSeed));
                }

                auto noise = noiseGen.sampler();
                __m128i originMM = _mm_setr_epi32(origin.x(), origin.y(), 0, 0);
                float noiseScale = 10.f / parameters_.scale;

                for (int y = 0; y < size; ++y) {
                    for (int x = 0; x < size; ++x) {
                        int cx = (x << 1) - size + 1;
                        int cy = (y << 1) - size + 1;
                        float sq = 1.f - sqrtf(cx * cx + cy * cy) * scale;
                        float alt;
                        if (sq <= 0.f) {
                            alt = 0.f;
                        } else {
                            auto posI = _mm_add_epi32(_mm_setr_epi32(x, y, 0, 0), originMM);
                            auto pos = _mm_cvtepi32_ps(posI);
                            pos = _mm_mul_ps(pos, _mm_set1_ps(noiseScale));
                            auto pos1 = _mm_mul_ps(pos, _mm_set1_ps(0.1f));
                            pos = _mm_unpacklo_ps(_mm_hadd_ps(pos, pos), _mm_hsub_ps(pos, pos));
                            auto pos2 = _mm_mul_ps(pos, _mm_set1_ps(0.15f));
                            auto pos3 = _mm_mul_ps(pos, _mm_set1_ps(0.3f));
                            auto pos4 = _mm_mul_ps(pos, _mm_set1_ps(0.03f));
                            float noiseVal = noise.sample(pos1);
                            noiseVal += noise.sample(pos2) * .3f;
                            noiseVal += noise.sample(pos3) * .15f;
                            noiseVal += noise.sample(pos4) * 1.5f;
                            noiseVal = std::max(std::min(0.5f + noiseVal * 1.1f, 1.f), 0.f);

                            float sqBase = sq;
                            sq *= sq * (3.f - 2.f * sq) * 0.8f;
                            sq *= sq;
                            sq -= 0.1f;
                            sq += (sqBase - sq) * std::abs(noiseVal);
                            alt = std::max(0.f, sq);
                        }
                        t->landform(x, y) = alt;
                    }
                }
            }
            break;
        case BrushTipType::Bell:
            for (int y = 0; y < size; ++y) {
                for (int x = 0; x < size; ++x) {
                    int cx = (x << 1) - size + 1;
                    int cy = (y << 1) - size + 1;
                    float sq = 1.f - sqrtf(cx * cx + cy * cy) * scale;
                    float alt;
                    if (sq <= 0.f) {
                        alt = 0.f;
                    } else {
                        sq *= sq * (3.f - 2.f * sq);
                        alt = sq;
                    }
                    t->landform(x, y) = alt;
                }
            }
            break;
        case BrushTipType::Cone:
            for (int y = 0; y < size; ++y) {
                for (int x = 0; x < size; ++x) {
                    int cx = (x << 1) - size + 1;
                    int cy = (y << 1) - size + 1;
                    float sq = 1.f - sqrtf(cx * cx + cy * cy) * scale;
                    float alt;
                    if (sq <= 0.f) {
                        alt = 0.f;
                    } else {
                        alt = sq;
                    }
                    t->landform(x, y) = alt;
                }
            }
            break;
        case BrushTipType::Sphere:
            scale *= scale;
            for (int y = 0; y < size; ++y) {
                for (int x = 0; x < size; ++x) {
                    int cx = (x << 1) - size + 1;
                    int cy = (y << 1) - size + 1;
                    float sq = 1.f - (cx * cx + cy * cy) * scale;
                    float alt;
                    if (sq <= 0.f) {
                        alt = 0.f;
                    } else {
                        alt = std::sqrt(sq);
                    }
                    t->landform(x, y) = alt;
                }
            }
            break;
        case BrushTipType::Cylinder:
            for (int y = 0; y < size; ++y) {
                for (int x = 0; x < size; ++x) {
                    int cx = (x << 1) - size + 1;
                    int cy = (y << 1) - size + 1;
                    float sq = size * size - (cx * cx + cy * cy);
                    float alt;
                    if (sq <= 0.f) {
                        alt = 0.f;
                    } else {
                        alt = 1.f;
                    }
                    t->landform(x, y) = alt;
                }
            }
            break;
        case BrushTipType::Square:
            for (int y = 0; y < size; ++y) {
                for (int x = 0; x < size; ++x) {
                    t->landform(x, y) = 1.f;
                }
            }
            break;
        }

    }
    return tip_;
}

BrushToolEdit *BrushTool::edit(TerrainEdit *edit, Terrain *terrain)
{
    return new BrushToolEdit(edit, terrain, this);
}

void BrushTool::discardTip()
{
    tip_.reset();
}

BrushToolEdit::BrushToolEdit(TerrainEdit *edit, Terrain *terrain, BrushTool *tool) :
    tool(tool),
    edit(edit),
    terrain(terrain),
    params(tool->parameters())
{

}


BrushToolEdit::~BrushToolEdit()
{

}

template <class Map>
void BrushToolEdit::drawRaiseLower(const QPoint &pt, Map &&map)
{
    Terrain *tip = tool->tip(pt).data();

    // compute affected rectangle
    QRect dirtyRect(pt, tip->size());
    dirtyRect = dirtyRect.intersected(QRect(QPoint(0, 0), terrain->size()));

    if (!dirtyRect.isValid()) {
        return;
    }

    // get "before" terrain
    if (tool->before_ == nullptr || tool->before_->size() != tip->size()) {
        tool->before_ = QSharedPointer<Terrain>::create(tip->size());
    }
    Terrain *before = tool->before_.data();
    edit->copyBeforeTo(dirtyRect, terrain, before);

    edit->beginEdit(dirtyRect, terrain);
    for (int y = 0; y < dirtyRect.height(); ++y) {
        for (int x = 0; x < dirtyRect.width(); ++x) {
            int cx = x + dirtyRect.left();
            int cy = y + dirtyRect.top();
            int tx = cx - pt.x();
            int ty = cy - pt.y();
            map(terrain->landform(cx, cy), before->landform(x, y), tip->landform(tx, ty));
        }
    }
    edit->endEdit(terrain);
}

template <class Map>
void BrushToolEdit::drawColor(const QPoint &pt, Map &&map)
{
    Terrain *tip = tool->tip(pt).data();

    // compute affected rectangle
    QRect dirtyRect(pt, tip->size());
    dirtyRect = dirtyRect.intersected(QRect(QPoint(0, 0), terrain->size()));

    if (!dirtyRect.isValid()) {
        return;
    }

    // get "before" terrain
    if (tool->before_ == nullptr || tool->before_->size() != tip->size()) {
        tool->before_ = QSharedPointer<Terrain>::create(tip->size());
    }
    Terrain *before = tool->before_.data();
    edit->copyBeforeTo(dirtyRect, terrain, before);

    edit->beginEdit(dirtyRect, terrain);
    for (int y = 0; y < dirtyRect.height(); ++y) {
        for (int x = 0; x < dirtyRect.width(); ++x) {
            int cx = x + dirtyRect.left();
            int cy = y + dirtyRect.top();
            int tx = cx - pt.x();
            int ty = cy - pt.y();
            map(terrain->color(cx, cy), before->color(x, y), tip->landform(tx, ty));
        }
    }
    edit->endEdit(terrain);
}

struct GaussianKernelTable
{
    __m128 table[65]; // (sigma^2) = {0 ... 4}
    // for 1d gaussian kernel
    // [v1, v2, v3, v4, v5, v6, v7]
    // table entry contains:
    // [v4 / 2, v3, v2, v1] == [v4 / 2, v5, v6, v7]

    GaussianKernelTable()
    {
        for (int i = 0; i <= 64; ++i) {
            float sigmaSqr = i / 16.f + .0001f;
            float kernel[4];
            for (int k = 0; k < 4; ++k) {
                kernel[k] = std::exp(-(k * k) / (2.f * sigmaSqr));
            }
            kernel[0] *= 0.5f;

            float totalEnergy = std::accumulate(kernel, kernel + 4, 0.f);
            float scale = 0.5f / totalEnergy;
            for (auto &k: kernel) k *= scale;

            table[i] = _mm_loadu_ps(kernel);
        }
    }

    inline __m128 fetch(float sigmaSqr) {
        int i = static_cast<int>(sigmaSqr * 16.f + .5f);
        i = std::min(i, 64);
        return _mm_loadu_ps(reinterpret_cast<const float *>(table + i));
    }
};
static GaussianKernelTable globalGaussianKernelTable;

void BrushToolEdit::drawBlur(const QPoint &pt, float amount)
{
    Terrain *tip = tool->tip(pt).data();

    // compute affected rectangle
    QRect dirtyRect(pt, tip->size());
    dirtyRect = dirtyRect.intersected(QRect(QPoint(0, 0), terrain->size()));

    if (!dirtyRect.isValid()) {
        return;
    }

    edit->beginEdit(dirtyRect, terrain);

    QSize tSize = terrain->size();
    QSize tipSize = tip->size();
    QSize blurBufferSize(dirtyRect.width() + 6, dirtyRect.height() + 6);
    TemporaryBuffer<__m128> blurBuffer1(blurBufferSize.width() * blurBufferSize.height(), 16);
    TemporaryBuffer<__m128> blurBuffer2(blurBufferSize.width() * blurBufferSize.height(), 16);
    TemporaryBuffer<float> tipBuffer(blurBufferSize.width() * blurBufferSize.height(), 4);

    for (int y = 0; y < blurBufferSize.height(); ++y) {
        int cy = y + dirtyRect.top() - 3;
        cy = std::max(std::min(cy, tSize.height() - 1), 0);
        for (int x = 0; x < blurBufferSize.width(); ++x) {
            int cx = x + dirtyRect.left() - 3;
            cx = std::max(std::min(cx, tSize.width() - 1), 0);

            quint32 color = terrain->color(cx, cy);
            auto colorMM = _mm_setr_epi32(color, 0, 0, 0);
            colorMM = _mm_unpacklo_epi8(colorMM, _mm_setzero_si128());
            colorMM = _mm_unpacklo_epi16(colorMM, _mm_setzero_si128());
            auto colorF = _mm_cvtepi32_ps(colorMM);
            _mm_store_ps(reinterpret_cast<float *>(blurBuffer1 + x + y * blurBufferSize.width()), colorF);
        }
    }
    for (int y = 0; y < blurBufferSize.height(); ++y) {
        int cy = y + dirtyRect.top() - 3;
        int ty = cy - pt.y();
        if (ty >= 0 && ty < tipSize.height()) {
            for (int x = 0; x < blurBufferSize.width(); ++x) {
                int cx = x + dirtyRect.left() - 3;
                int tx = cx - pt.x();
                tipBuffer[x + y * blurBufferSize.width()] =
                        tx >= 0 && tx < tipSize.width() ?
                            tip->landform(tx, ty) * amount :
                            0.f;
            }
        } else {
            std::fill(&tipBuffer[y * blurBufferSize.width()],
                    &tipBuffer[(y + 1) * blurBufferSize.width()], 0.f);
        }
    }

    // apply horizontal blur
    for (int y = 0; y < blurBufferSize.height(); ++y) {
        __m128 *inBuf = blurBuffer1 + y * blurBufferSize.width();
        __m128 *outBuf = blurBuffer2 + y * blurBufferSize.width();
        float *varBuf = tipBuffer + y * blurBufferSize.width();
        for (int x = 3; x < blurBufferSize.width() - 3; ++x) {
            float variance = varBuf[x];
            __m128 kernel = globalGaussianKernelTable.fetch(variance);

            // sample input
            __m128 p1 = _mm_load_ps(reinterpret_cast<float *>(inBuf + x));
            p1 = _mm_add_ps(p1, p1);
            __m128 p2 = _mm_load_ps(reinterpret_cast<float *>(inBuf + x + 1));
            p2 = _mm_add_ps(p2, _mm_load_ps(reinterpret_cast<float *>(inBuf + x - 1)));
            __m128 p3 = _mm_load_ps(reinterpret_cast<float *>(inBuf + x + 2));
            p3 = _mm_add_ps(p3, _mm_load_ps(reinterpret_cast<float *>(inBuf + x - 2)));
            __m128 p4 = _mm_load_ps(reinterpret_cast<float *>(inBuf + x + 3));
            p4 = _mm_add_ps(p4, _mm_load_ps(reinterpret_cast<float *>(inBuf + x - 3)));

            // apply kernel
            p1 = _mm_mul_ps(p1, _mm_shuffle_ps(kernel, kernel, _MM_SHUFFLE(0, 0, 0, 0)));
            p2 = _mm_mul_ps(p2, _mm_shuffle_ps(kernel, kernel, _MM_SHUFFLE(1, 1, 1, 1)));
            p3 = _mm_mul_ps(p3, _mm_shuffle_ps(kernel, kernel, _MM_SHUFFLE(2, 2, 2, 2)));
            p4 = _mm_mul_ps(p4, _mm_shuffle_ps(kernel, kernel, _MM_SHUFFLE(3, 3, 3, 3)));

            p1 = _mm_add_ps(p1, p2);
            p3 = _mm_add_ps(p3, p4);
            auto p = _mm_add_ps(p1, p3);

            // store
            _mm_store_ps(reinterpret_cast<float *>(outBuf + x), p);
        }
    }

    // apply vertical blur
    for (int y = 3; y < blurBufferSize.height() - 3; ++y) {
        __m128 *inBuf = blurBuffer2 + y * blurBufferSize.width();
        __m128 *outBuf = blurBuffer1 + y * blurBufferSize.width();
        float *varBuf = tipBuffer + y * blurBufferSize.width();
        for (int x = 3; x < blurBufferSize.width() - 3; x += 1) {
            // fetch kernel
            __m128 kernel = globalGaussianKernelTable.fetch(varBuf[x]);

            // load input
            __m128 p1 = _mm_load_ps(reinterpret_cast<float *>(inBuf + x));
            p1 = _mm_add_ps(p1, p1);
            __m128 p2 = _mm_load_ps(reinterpret_cast<float *>(inBuf + x - blurBufferSize.width()));
            p2 = _mm_add_ps(p2, _mm_load_ps(reinterpret_cast<float *>(inBuf + x + blurBufferSize.width())));
            __m128 p3 = _mm_load_ps(reinterpret_cast<float *>(inBuf + x - blurBufferSize.width() * 2));
            p3 = _mm_add_ps(p3, _mm_load_ps(reinterpret_cast<float *>(inBuf + x + blurBufferSize.width() * 2)));
            __m128 p4 = _mm_load_ps(reinterpret_cast<float *>(inBuf + x - blurBufferSize.width() * 3));
            p4 = _mm_add_ps(p4, _mm_load_ps(reinterpret_cast<float *>(inBuf + x + blurBufferSize.width() * 3)));

            // apply kernel
            p1 = _mm_mul_ps(p1, _mm_shuffle_ps(kernel, kernel, _MM_SHUFFLE(0, 0, 0, 0)));
            p2 = _mm_mul_ps(p2, _mm_shuffle_ps(kernel, kernel, _MM_SHUFFLE(1, 1, 1, 1)));
            p3 = _mm_mul_ps(p3, _mm_shuffle_ps(kernel, kernel, _MM_SHUFFLE(2, 2, 2, 2)));
            p4 = _mm_mul_ps(p4, _mm_shuffle_ps(kernel, kernel, _MM_SHUFFLE(3, 3, 3, 3)));
            p1 = _mm_add_ps(p1, p2);
            p3 = _mm_add_ps(p3, p4);
            auto p = _mm_add_ps(p1, p3);

            // store
            _mm_store_ps(reinterpret_cast<float *>(outBuf + x), p);
        }
    }

    for (int y = 0; y < dirtyRect.height(); ++y) {
        __m128 *inBuf = blurBuffer1 + (y + 3) * blurBufferSize.width() + 3;
        for (int x = 0; x < dirtyRect.width(); ++x) {
            int cx = x + dirtyRect.left();
            int cy = y + dirtyRect.top();
            auto colorF = _mm_load_ps(reinterpret_cast<float *>(inBuf + x));
            colorF = _mm_add_ps(colorF, _mm_set1_ps(0.5f));
            colorF = _mm_add_ps(colorF, globalDitherSampler.getM128());
            auto colorMM = _mm_cvttps_epi32(colorF);
            colorMM = _mm_packs_epi32(colorMM, colorMM);
            colorMM = _mm_packus_epi16(colorMM, colorMM);
            _mm_store_ss(reinterpret_cast<float*>(&terrain->color(cx, cy)), _mm_castsi128_ps(colorMM));
        }
    }
    edit->endEdit(terrain);
}

void BrushToolEdit::drawSmoothen(const QPoint &pt, float amount)
{
    Terrain *tip = tool->tip(pt).data();

    // compute affected rectangle
    QRect dirtyRect(pt, tip->size());
    dirtyRect = dirtyRect.intersected(QRect(QPoint(0, 0), terrain->size()));

    if (!dirtyRect.isValid()) {
        return;
    }

    edit->beginEdit(dirtyRect, terrain);

    QSize tSize = terrain->size();
    QSize tipSize = tip->size();
    QSize blurBufferSize(dirtyRect.width() + 6, dirtyRect.height() + 6);
    TemporaryBuffer<float> blurBuffer1(blurBufferSize.width() * blurBufferSize.height(), 4);
    TemporaryBuffer<float> blurBuffer2(blurBufferSize.width() * blurBufferSize.height(), 4);
    TemporaryBuffer<float> tipBuffer(blurBufferSize.width() * blurBufferSize.height(), 4);

    for (int y = 0; y < blurBufferSize.height(); ++y) {
        int cy = y + dirtyRect.top() - 3;
        cy = std::max(std::min(cy, tSize.height() - 1), 0);
        for (int x = 0; x < blurBufferSize.width(); ++x) {
            int cx = x + dirtyRect.left() - 3;
            cx = std::max(std::min(cx, tSize.width() - 1), 0);
            blurBuffer1[x + y * blurBufferSize.width()] =
                    terrain->landform(cx, cy);
        }
    }
    for (int y = 0; y < blurBufferSize.height(); ++y) {
        int cy = y + dirtyRect.top() - 3;
        int ty = cy - pt.y();
        if (ty >= 0 && ty < tipSize.height()) {
            for (int x = 0; x < blurBufferSize.width(); ++x) {
                int cx = x + dirtyRect.left() - 3;
                int tx = cx - pt.x();
                tipBuffer[x + y * blurBufferSize.width()] =
                        tx >= 0 && tx < tipSize.width() ?
                            tip->landform(tx, ty) * amount :
                            0.f;
            }
        } else {
            std::fill(&tipBuffer[y * blurBufferSize.width()],
                    &tipBuffer[(y + 1) * blurBufferSize.width()], 0.f);
        }
    }

    // apply horizontal blur
    for (int y = 0; y < blurBufferSize.height(); ++y) {
        float *inBuf = blurBuffer1 + y * blurBufferSize.width();
        float *outBuf = blurBuffer2 + y * blurBufferSize.width();
        float *varBuf = tipBuffer + y * blurBufferSize.width();
        for (int x = 3; x < blurBufferSize.width() - 3; ++x) {
            float variance = varBuf[x];
            __m128 kernel = globalGaussianKernelTable.fetch(variance);

            // sample input
            __m128 p1 = _mm_loadu_ps(inBuf + x - 3);
            __m128 p2 = _mm_loadu_ps(inBuf + x);
            p1 = _mm_shuffle_ps(p1, p1, _MM_SHUFFLE(0, 1, 2, 3));
            auto p = _mm_add_ps(p1, p2);

            // apply kernel
            p = _mm_mul_ps(p, kernel);
            p = _mm_hadd_ps(p, p);
            p = _mm_hadd_ps(p, p);

            // write
            _mm_store_ss(outBuf + x, p);
        }
    }

    // apply vertical blur
    for (int y = 3; y < blurBufferSize.height() - 3; ++y) {
        float *inBuf = blurBuffer2 + y * blurBufferSize.width();
        float *outBuf = blurBuffer1 + y * blurBufferSize.width();
        float *varBuf = tipBuffer + y * blurBufferSize.width();
        for (int x = 0; x < blurBufferSize.width() - 3; x += 4) {
            // fetch kernel
            __m128 kernel1 = globalGaussianKernelTable.fetch(varBuf[x]);
            __m128 kernel2 = globalGaussianKernelTable.fetch(varBuf[x + 1]);
            __m128 kernel3 = globalGaussianKernelTable.fetch(varBuf[x + 2]);
            __m128 kernel4 = globalGaussianKernelTable.fetch(varBuf[x + 3]);
            _MM_TRANSPOSE4_PS(kernel1, kernel2, kernel3, kernel4);

            // load input
            __m128 p1 = _mm_loadu_ps(inBuf + x);
            p1 = _mm_add_ps(p1, p1);
            __m128 p2 = _mm_loadu_ps(inBuf + x - blurBufferSize.width());
            p2 = _mm_add_ps(p2, _mm_loadu_ps(inBuf + x + blurBufferSize.width()));
            __m128 p3 = _mm_loadu_ps(inBuf + x - blurBufferSize.width() * 2);
            p3 = _mm_add_ps(p3, _mm_loadu_ps(inBuf + x + blurBufferSize.width() * 2));
            __m128 p4 = _mm_loadu_ps(inBuf + x - blurBufferSize.width() * 3);
            p4 = _mm_add_ps(p4, _mm_loadu_ps(inBuf + x + blurBufferSize.width() * 3));

            // apply kernel
            p1 = _mm_mul_ps(p1, kernel1);
            p2 = _mm_mul_ps(p2, kernel2);
            p3 = _mm_mul_ps(p3, kernel3);
            p4 = _mm_mul_ps(p4, kernel4);
            p1 = _mm_add_ps(p1, p2);
            p3 = _mm_add_ps(p3, p4);
            auto p = _mm_add_ps(p1, p3);

            // store
            _mm_storeu_ps(outBuf + x, p);
        }
    }

    for (int y = 0; y < dirtyRect.height(); ++y) {
        float *inBuf = blurBuffer1 + (y + 3) * blurBufferSize.width() + 3;
        for (int x = 0; x < dirtyRect.width(); ++x) {
            int cx = x + dirtyRect.left();
            int cy = y + dirtyRect.top();
            terrain->landform(cx, cy) = inBuf[x];
        }
    }
    edit->endEdit(terrain);
}

enum class BrushToolEdit::FeatureLevel
{
    Sse2
};

void BrushToolEdit::draw(const QPoint &pt, float strength)
{
    drawInner<FeatureLevel::Sse2>(pt, strength);
}

template<BrushToolEdit::FeatureLevel level>
void BrushToolEdit::drawInner(const QPoint &pt, float strength)
{
    float fixedStrength = params.strength;
    strength *= fixedStrength;

    auto color = params.color;
    std::array<int, 3> colorParts = Terrain::expandColor(color);
    __m128 colorMM = _mm_setr_ps(colorParts[0], colorParts[1], colorParts[2], 0);

    SseRoundingModeScope roundingModeScope(_MM_ROUND_NEAREST);
    (void) roundingModeScope;

    switch (tool->type()) {
    case BrushType::Blur:
        drawBlur(pt, std::min(strength / 5.f, 4.f));
        break;
    case BrushType::Smoothen:
        drawSmoothen(pt, std::min(strength / 5.f, 4.f));
        break;
    case BrushType::Raise:
    case BrushType::Lower:
        if (tool->type() == BrushType::Lower) {
            fixedStrength = -fixedStrength;
            strength = -strength;
        }
        switch (params.pressureMode) {
        case BrushPressureMode::AirBrush:
            strength *= 3.f;
            drawRaiseLower(pt, [=](float &current, float before, float tip) {
                (void) before;
                current -= tip * strength;
            });
            break;
        case BrushPressureMode::Constant:
            if (tool->type() == BrushType::Lower) {
                drawRaiseLower(pt, [=](float &current, float before, float tip) {
                    current = Terrain::quantizeOne(std::max(current, before - tip * fixedStrength));
                });
            } else {
                drawRaiseLower(pt, [=](float &current, float before, float tip) {
                    current = Terrain::quantizeOne(std::min(current, before - tip * fixedStrength));
                });
            }
            break;
        case BrushPressureMode::Adjustable:
            drawRaiseLower(pt, [=](float &current, float before, float tip) {
                current = Terrain::quantizeOne(before - tip * strength);
            });
            break;
        }
        break;
    case BrushType::Paint:
        switch (params.pressureMode) {
        case BrushPressureMode::AirBrush:
            strength = 1.f - std::exp2(-strength);

            drawColor(pt, [=](quint32 &current, quint32 before, float tip) {
                (void) before;

                // convert current color to FP32
                auto currentMM = _mm_castps_si128(_mm_load_ss(reinterpret_cast<float *>(&current)));
                currentMM = _mm_unpacklo_epi8(currentMM, _mm_setzero_si128());
                currentMM = _mm_unpacklo_epi16(currentMM, _mm_setzero_si128());
                auto currentMF = _mm_cvtepi32_ps(currentMM);

                auto factor = _mm_set1_ps(tip * strength);

                // blend
                auto diff = _mm_sub_ps(colorMM, currentMF);
                diff = _mm_mul_ps(diff, factor);
                currentMF = _mm_add_ps(currentMF, diff);

                // convert to RGB32
                currentMF = _mm_add_ps(currentMF, globalDitherSampler.getM128());
                currentMM = _mm_cvttps_epi32(currentMF);
                currentMM = _mm_packs_epi32(currentMM, currentMM);
                currentMM = _mm_packus_epi16(currentMM, currentMM);

                _mm_store_ss(reinterpret_cast<float *>(&current), _mm_castsi128_ps(currentMM));
            });
            break;
        case BrushPressureMode::Constant:
            fixedStrength *= 0.01f;
            drawColor(pt, [=](quint32 &current, quint32 before, float tip) {
                // convert current color to FP32
                auto currentMM = _mm_castps_si128(_mm_load_ss(reinterpret_cast<float *>(&current)));
                currentMM = _mm_unpacklo_epi8(currentMM, _mm_setzero_si128());
                currentMM = _mm_unpacklo_epi16(currentMM, _mm_setzero_si128());
                auto currentMF = _mm_cvtepi32_ps(currentMM);

                // convert before color to FP32
                auto beforeMM = _mm_setr_epi32(before, 0, 0, 0);
                beforeMM = _mm_unpacklo_epi8(beforeMM, _mm_setzero_si128());
                beforeMM = _mm_unpacklo_epi16(beforeMM, _mm_setzero_si128());
                auto beforeMF = _mm_cvtepi32_ps(beforeMM);
                // beforeMM = _mm_add_ps(beforeMM, globalDitherSampler.getM128());

                // use "before" image to which way of color change is possible, and
                // compute possible range of result color
                auto diff = _mm_sub_ps(colorMM, beforeMF);
                auto factor = _mm_set1_ps(tip * fixedStrength);
                auto adddiff = _mm_mul_ps(diff, factor);
                beforeMF = _mm_add_ps(beforeMF, adddiff);
                auto diffDir = _mm_cmpgt_ps(diff, _mm_setzero_ps());

                // compute output image
                auto out1 = _mm_max_ps(currentMF, beforeMF);
                auto out2 = _mm_min_ps(currentMF, beforeMF);
                currentMF = _mm_or_ps(_mm_and_ps(diffDir, out1), _mm_andnot_ps(diffDir, out2));

                // convert to RGB32
                currentMF = _mm_add_ps(currentMF, globalDitherSampler.getM128());
                currentMM = _mm_cvttps_epi32(currentMF);
                currentMM = _mm_packs_epi32(currentMM, currentMM);
                currentMM = _mm_packus_epi16(currentMM, currentMM);

                _mm_store_ss(reinterpret_cast<float *>(&current), _mm_castsi128_ps(currentMM));
            });
            break;
        case BrushPressureMode::Adjustable:
            strength *= 0.01f;
            drawColor(pt, [=](quint32 &current, quint32 before, float tip) {

                // convert before color to FP32
                auto beforeMM = _mm_setr_epi32(before, 0, 0, 0);
                beforeMM = _mm_unpacklo_epi8(beforeMM, _mm_setzero_si128());
                beforeMM = _mm_unpacklo_epi16(beforeMM, _mm_setzero_si128());
                auto beforeMF = _mm_cvtepi32_ps(beforeMM);

                // blend
                auto diff = _mm_sub_ps(colorMM, beforeMF);
                auto factor = _mm_set1_ps(tip * strength);
                diff = _mm_mul_ps(diff, factor);
                beforeMF = _mm_add_ps(beforeMF, diff);

                // convert to RGB32
                beforeMF = _mm_add_ps(beforeMF, globalDitherSampler.getM128());
                beforeMM = _mm_cvttps_epi32(beforeMF);
                beforeMM = _mm_packs_epi32(beforeMM, beforeMM);
                beforeMM = _mm_packus_epi16(beforeMM, beforeMM);

                _mm_store_ss(reinterpret_cast<float *>(&current), _mm_castsi128_ps(beforeMM));
            });
            break;
        }
        break;
    }

}


