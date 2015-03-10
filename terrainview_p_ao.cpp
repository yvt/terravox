#include "terrainview_p.h"
#include <smmintrin.h>
#include "terrain.h"
#include <QtConcurrent>
#include <QMatrix4x4>
#include "temporarybuffer.h"

enum {
    AORange = 2
};

//
// AORange = 2
//             2
//         |---|.   1
//  . . .  |///| ---__   0
//         |///|    | ---.
// -------------------------
//  -1     0   1    2    3
// in this case, kernel [0.5, 1, 1, 0.5] is used.
// when extending this to 2D, you use the kernel similar to the following one:
//   [0.3, 0.5, 0.5, 0.3]
//   [0.5,   1,   1, 0.5]
//   [0.5,   1,   1, 0.5]
//   [0.3, 0.5, 0.5, 0.3]

// approximates ambient occlusion by solving problem similar to the maximum convoluion problem (MAXCON).
// (name acoording to Bussieck, Michael and Hassler, Hannes and Woeginger, Gerhard J. and Zimmermann, Uwe T.,
//  Fast Algorithms for the Maximum Convolution Problem, 1994.)

void TerrainView::TerrainViewPrivate::updateAmbientOcclusion()
{
    if (aoMapDirtyRect_.isNull()) {
        return;
    }

    int tWidth = terrain_->size().width();
    int tHeight = terrain_->size().height();
    int aoMapWidth = terrain_->size().width() + 1;
    int aoMapHeight = terrain_->size().height() + 1;
    QRect updateRect = aoMapDirtyRect_.adjusted(1 - AORange, 1 - AORange, AORange, AORange);
    updateRect = updateRect.intersected(QRect(0, 0, aoMapWidth, aoMapHeight));

    aoMap_.resize(aoMapWidth * aoMapHeight);

    static_assert(AORange == 2, "this code assumes AORange == 2");

    float *landform = terrain_->landformData();
    float *aoMap = aoMap_.data();

    auto kernel1 = _mm_setr_ps(0.292f, 0.5f, 0.5f, 0.292f);
    auto kernel2 = _mm_setr_ps(0.5f, 1.f, 1.f, 0.5f);
    auto kernel3 = kernel2;
    auto kernel4 = kernel1;

    Q_ASSERT(tWidth >= 8); // another assumption

    int maxX = std::min(updateRect.right(), tWidth - 2);

    const float Bottom = 64.f;

    TemporaryBuffer<float> dummyRow(tWidth);
    auto *dummyRowData = dummyRow.get();
    std::fill(dummyRowData, dummyRowData + tWidth, Bottom);

    QtConcurrent::blockingMap(
                RangeIterator(updateRect.top() >> 4),
                RangeIterator((updateRect.bottom() >> 4) + 1),
                [=](int block)
    {
        int minY = std::max(updateRect.top(), block << 4);
        int maxY = std::min(updateRect.bottom(), (block << 4) + 15);
        for (int y = minY; y <= maxY; ++y) {
            float *inLand1 = y < 2 ? dummyRowData : landform + (y - 2) * tWidth;
            float *inLand2 = y < 1 ? dummyRowData : landform + (y - 1) * tWidth;
            float *inLand3 =                          landform + (y)     * tWidth;
            float *inLand4 = y >= tHeight - 1 ? dummyRowData : landform + (y + 1) * tWidth;
            float *outMap = aoMap + y * aoMapWidth;

            auto convolute = [kernel1, kernel2, kernel3, kernel4](__m128 in1, __m128 in2, __m128 in3, __m128 in4) -> __m128
            {
                // compute reference plane
                auto ref1 = _mm_shuffle_ps(in2, in3, _MM_SHUFFLE(2, 1, 2, 1));
                auto ref2 = _mm_min_ps(ref1, _mm_shuffle_ps(ref1, ref1, _MM_SHUFFLE(1, 0, 3, 2)));
                auto ref =  _mm_min_ps(ref2, _mm_shuffle_ps(ref2, ref2, _MM_SHUFFLE(2, 3, 0, 1)));

                // subtract reference plane
                in1 = _mm_sub_ps(in1, ref); in1 = _mm_min_ps(in1, _mm_setzero_ps());
                in2 = _mm_sub_ps(in2, ref); in2 = _mm_min_ps(in2, _mm_setzero_ps());
                in3 = _mm_sub_ps(in3, ref); in3 = _mm_min_ps(in3, _mm_setzero_ps());
                in4 = _mm_sub_ps(in4, ref); in4 = _mm_min_ps(in4, _mm_setzero_ps());

                // weight
                in1 = _mm_mul_ps(in1, kernel1);
                in2 = _mm_mul_ps(in2, kernel2);
                in3 = _mm_mul_ps(in3, kernel3);
                in4 = _mm_mul_ps(in4, kernel4);

                // convolute
                auto a1 = _mm_min_ps(in1, in2);
                auto a2 = _mm_min_ps(in3, in4);
                auto b = _mm_min_ps(a1, a2);
                auto c = _mm_min_ps(b, _mm_shuffle_ps(b, b, _MM_SHUFFLE(2, 3, 0, 1)));
                auto d = _mm_min_ps(c, _mm_shuffle_ps(c, c, _MM_SHUFFLE(1, 0, 3, 2)));

                // restore reference plane
                d = _mm_add_ss(d, ref);

                return d;
            };

            int x = updateRect.left();
            if (x == 0) {
                auto in1 = _mm_loadh_pi(_mm_set1_ps(Bottom), reinterpret_cast<const __m64*>(inLand1));
                auto in2 = _mm_loadh_pi(_mm_set1_ps(Bottom), reinterpret_cast<const __m64*>(inLand2));
                auto in3 = _mm_loadh_pi(_mm_set1_ps(Bottom), reinterpret_cast<const __m64*>(inLand3));
                auto in4 = _mm_loadh_pi(_mm_set1_ps(Bottom), reinterpret_cast<const __m64*>(inLand4));
                auto result = convolute(in1, in2, in3, in4);
                _mm_store_ss(outMap, result);
                ++x;
            }
            if (x == 1) {
                auto in1 = _mm_loadu_ps(inLand1); in1 = _mm_castsi128_ps(_mm_slli_si128(_mm_castps_si128(in1), 4));
                auto in2 = _mm_loadu_ps(inLand2); in2 = _mm_castsi128_ps(_mm_slli_si128(_mm_castps_si128(in2), 4));
                auto in3 = _mm_loadu_ps(inLand3); in3 = _mm_castsi128_ps(_mm_slli_si128(_mm_castps_si128(in3), 4));
                auto in4 = _mm_loadu_ps(inLand4); in4 = _mm_castsi128_ps(_mm_slli_si128(_mm_castps_si128(in4), 4));
                in1 = _mm_move_ss(in1, _mm_set_ss(Bottom));
                in2 = _mm_move_ss(in2, _mm_set_ss(Bottom));
                in3 = _mm_move_ss(in3, _mm_set_ss(Bottom));
                in4 = _mm_move_ss(in4, _mm_set_ss(Bottom));
                auto result = convolute(in1, in2, in3, in4);
                _mm_store_ss(outMap + 1, result);
                ++x;
            }
            while (x <= maxX) {
                auto in1 = _mm_loadu_ps(inLand1 + x - 2);
                auto in2 = _mm_loadu_ps(inLand2 + x - 2);
                auto in3 = _mm_loadu_ps(inLand3 + x - 2);
                auto in4 = _mm_loadu_ps(inLand4 + x - 2);
                auto result = convolute(in1, in2, in3, in4);
                _mm_store_ss(outMap + x, result);
                ++x;
            }
            if (maxX != updateRect.right()) {
                auto in1 = _mm_loadu_ps(inLand1 + x - 3);
                auto in2 = _mm_loadu_ps(inLand2 + x - 3);
                auto in3 = _mm_loadu_ps(inLand3 + x - 3);
                auto in4 = _mm_loadu_ps(inLand4 + x - 3);
                in1 = _mm_move_ss(in1, _mm_set_ss(Bottom));
                in2 = _mm_move_ss(in2, _mm_set_ss(Bottom));
                in3 = _mm_move_ss(in3, _mm_set_ss(Bottom));
                in4 = _mm_move_ss(in4, _mm_set_ss(Bottom));
                in1 = _mm_shuffle_ps(in1, in1, _MM_SHUFFLE(0, 3, 2, 1));
                in2 = _mm_shuffle_ps(in2, in2, _MM_SHUFFLE(0, 3, 2, 1));
                in3 = _mm_shuffle_ps(in3, in3, _MM_SHUFFLE(0, 3, 2, 1));
                in4 = _mm_shuffle_ps(in4, in4, _MM_SHUFFLE(0, 3, 2, 1));
                auto result = convolute(in1, in2, in3, in4);
                _mm_store_ss(outMap + x, result);
            }
        }
    });

    aoMapDirtyRect_ = QRect();
}

void TerrainView::TerrainViewPrivate::applyAmbientOcclusion()
{
    updateAmbientOcclusion();

    const int imgWidth = image_->size().width();
    const int imgHeight = image_->size().height();
    quint32 *colorOutput = transposedImage_.data();
    float *depthInput = depthImage_.data();
    float *heightInput = heightImage_.data();

    Q_ASSUME(imgHeight >= 8);
    Q_ASSUME(imgHeight % 4 == 0);

    int tWidth = terrain_->size().width();
    int tHeight = terrain_->size().height();
    int aoMapWidth = terrain_->size().width() + 1;
    int aoMapHeight = terrain_->size().height() + 1;
    float *aoMap = aoMap_.data();

    // here's description of approximated ambient occlusion by using heightmap of ambient occlusion volume:
    //   1. compute global position p = (x, y, z) for each framebuffer pixel
    //
    //      Value stored in depth buffer is computed as following:
    //        depthValue = dot(p, cameraDir2D) where p is the global position
    //      Therefore,
    //        A := dot(p, cameraDir2D) = depthValue ... (1)
    //      Y screen coordinate of the point p can be expressed as following:
    //        screenY = (image.height >> 1) + dot(p - eye, upVector) * (image.height / viewHeight)
    //      Therefore,
    //        B := dot(p, upVector) = (screenY - (image.height >> 1)) * (viewHeight / image.height) + dot(eye, upVector) ... (2)
    //      And by using X screen coordinate:
    //        C := dot(p, rightVector) = (screenX - (image.width >> 1)) * (viewWidth / image.width) + dot(eye, rightVector) ... (2)
    //      From (1), (2) and (3), we can compute p as following:
    //        [ cameraDir2D ]     [ A ]
    //        [   upVector  ] p = [ B ]
    //        [ rightVector ]     [ C ]
    //
    //                        p = inverse(M) * transpose(A, B, C) where M = ...
    //
    //                                         [ depthValue                                                                         ]
    //                        p = inverse(M) * [ (screenY - (image.height >> 1)) * (viewHeight / image.height) + dot(eye, upVector) ]
    //                                         [ (screenX - (image.width >> 1)) * (viewWidth / image.width) + dot(eye, rightVector) ]
    //
    //     Beaware of cameraDir2D and upVector being likely to be nearly parallel, which would result in a numerical instability.
    //     We disable the AO in this case.
    //
    //   2. compute height of AO volume there
    //   3. compute difference between AO volume height and the height of the framebuffer pixel
    //   4. shade the framebuffer pixel

    float strength = 15.f - std::abs(QVector3D::dotProduct(upVector, cameraDir2D)) * 16.f;
    if (strength <= 0.f) {
        return;
    }
    strength = std::min(strength, 1.f) * viewOptions.ambientOcclusionStrength * 2.f;

    // compute matrix M
    QMatrix4x4 m(cameraDir2D.x(), cameraDir2D.y(), 0.f, 0.f,
                 upVector.x(), upVector.y(), upVector.z(), 0.f,
                 rightVector.x(), rightVector.y(), rightVector.z(), 0.f,
                 0.f, 0.f, 0.f, 1.f);

    bool success = false;
    m = m.inverted(&success);
    Q_ASSERT(success);


    // dp/d[screenX]
    QVector4D dpdx = m.column(2) * (sceneDef.viewWidth / imgWidth);
    auto dpdxMM = _mm_setr_ps(dpdx.x(), dpdx.y(), dpdx.z(), 0.f);

    // dp/d[screenY]
    QVector4D dpdy = m.column(1) * (sceneDef.viewHeight / imgHeight);
    auto dpdyMM = _mm_setr_ps(dpdy.x(), dpdy.y(), dpdy.z(), 0.f);

    // dp/d[depthValue]
    QVector4D dpdd = m.column(0);
    auto dpddMM = _mm_setr_ps(dpdd.x(), dpdd.y(), dpdd.z(), 0.f);

    // p at screenX = 0, screenY = 0, depthValue = 0
    float biasedZ = -(zBias + depthBufferBias);
    auto pBase = (m * QVector4D(
                      biasedZ,
                      -(imgHeight >> 1) * sceneDef.viewHeight / imgHeight + QVector3D::dotProduct(sceneDef.eye, upVector),
                      -(imgWidth >> 1) * sceneDef.viewWidth / imgWidth + QVector3D::dotProduct(sceneDef.eye, rightVector), 0.f));
    auto pBaseMM = _mm_setr_ps(pBase.x(), pBase.y(), pBase.z(), 0.f);

    // auto aoCoordMax = _mm_setr_ps(tWidth * 256.f - 2.f, tHeight * 256.f - 2.f, 0.f, 0.f); // work-around for border problem
    auto aoCoordMax = _mm_setr_ps(tWidth * 256.f - 256.f, tHeight * 256.f - 256.f, 0.f, 0.f);
    aoCoordMax = _mm_movelh_ps(aoCoordMax, aoCoordMax);

    // s24.8 fixed point (XY)
    auto mmScale = _mm_setr_ps(256.f, 256.f, 1.f, 0.f);
    dpdxMM = _mm_mul_ps(dpdxMM, mmScale);
    dpdyMM = _mm_mul_ps(dpdyMM, mmScale);
    dpddMM = _mm_mul_ps(dpddMM, mmScale);
    pBaseMM = _mm_mul_ps(pBaseMM, mmScale);


    QtConcurrent::blockingMap(RangeIterator(0), RangeIterator((imgWidth + 63) >> 6), [=](int block) {
        int minX = block << 6;
        int maxX = std::min(minX + 64, imgWidth);
        auto pColumn = _mm_add_ps(pBaseMM, _mm_mul_ps(dpdxMM, _mm_set1_ps(minX)));
        auto dpdyMM4 = _mm_mul_ps(dpdyMM, _mm_set1_ps(4.f));

        for (int x = minX; x < maxX; ++x) {
            quint32 *lineColorOutput = colorOutput + x * imgHeight;
            float *lineDepthInput = depthInput + x * imgHeight;
            float *lineHeightInput = heightInput + x * imgHeight;
            auto p = pColumn;
            pColumn = _mm_add_ps(pColumn, dpdxMM);

            for (int y = 0; y < imgHeight;
                 y += 4,
                 lineColorOutput += 4,
                 lineDepthInput += 4,
                 lineHeightInput += 4) {
                auto floorHeight = _mm_loadu_ps(lineHeightInput);

                // background cull
                auto isForeground = _mm_cmplt_ps(floorHeight, _mm_set1_ps(512.f));
                if (!_mm_movemask_ps(isForeground)) {
                    p = _mm_add_ps(p, dpdyMM4);
                    continue;
                }

                auto p1 = p;
                auto p2 = _mm_add_ps(p1, dpdyMM);
                auto p3 = _mm_add_ps(p2, dpdyMM);
                auto p4 = _mm_add_ps(p3, dpdyMM);
                p = _mm_add_ps(p, dpdyMM4);

                p1 = _mm_add_ps(p1, _mm_mul_ps(_mm_load1_ps(lineDepthInput), dpddMM));
                p2 = _mm_add_ps(p2, _mm_mul_ps(_mm_load1_ps(lineDepthInput + 1), dpddMM));
                p3 = _mm_add_ps(p3, _mm_mul_ps(_mm_load1_ps(lineDepthInput + 2), dpddMM));
                p4 = _mm_add_ps(p4, _mm_mul_ps(_mm_load1_ps(lineDepthInput + 3), dpddMM));

                // global coordinate XY * 256
                auto xyF1 = _mm_movelh_ps(p1, p2);
                auto xyF2 = _mm_movelh_ps(p3, p4);

                // global coordinate Z * 256
                auto terrainZ1 = _mm_movehl_ps(p2, p1);
                auto terrainZ2 = _mm_movehl_ps(p4, p3);
                auto terrainZ = _mm_shuffle_ps(terrainZ1, terrainZ2, _MM_SHUFFLE(2, 0, 2, 0));

                auto minAoVolumeHeight = _mm_sub_ps(terrainZ, _mm_set1_ps(AORange * 4.f));

                xyF1 = _mm_max_ps(xyF1, _mm_setzero_ps());
                xyF2 = _mm_max_ps(xyF2, _mm_setzero_ps());
                xyF1 = _mm_min_ps(xyF1, aoCoordMax);
                xyF2 = _mm_min_ps(xyF2, aoCoordMax);

                // global coordinate s24.8
                auto xyI1 = _mm_cvttps_epi32(xyF1);
                auto xyI2 = _mm_cvttps_epi32(xyF2);

                // global coordinate's integral part
                auto xyII1 = _mm_srli_epi32(xyI1, 8);
                auto xyII2 = _mm_srli_epi32(xyI2, 8);

                auto aoZ1 = _mm_setr_ps(
                        aoMap[_mm_extract_epi16(xyII1, 0) + (_mm_extract_epi16(xyII1, 2)) * aoMapWidth],
                        aoMap[_mm_extract_epi16(xyII1, 4) + (_mm_extract_epi16(xyII1, 6)) * aoMapWidth],
                        aoMap[_mm_extract_epi16(xyII2, 0) + (_mm_extract_epi16(xyII2, 2)) * aoMapWidth],
                        aoMap[_mm_extract_epi16(xyII2, 4) + (_mm_extract_epi16(xyII2, 6)) * aoMapWidth]);
                auto aoZ2 = _mm_setr_ps(
                        aoMap[_mm_extract_epi16(xyII1, 0) + 1 + (_mm_extract_epi16(xyII1, 2)) * aoMapWidth],
                        aoMap[_mm_extract_epi16(xyII1, 4) + 1 + (_mm_extract_epi16(xyII1, 6)) * aoMapWidth],
                        aoMap[_mm_extract_epi16(xyII2, 0) + 1 + (_mm_extract_epi16(xyII2, 2)) * aoMapWidth],
                        aoMap[_mm_extract_epi16(xyII2, 4) + 1 + (_mm_extract_epi16(xyII2, 6)) * aoMapWidth]);
                auto aoZ3 = _mm_setr_ps(
                        aoMap[_mm_extract_epi16(xyII1, 0) + (_mm_extract_epi16(xyII1, 2) + 1) * aoMapWidth],
                        aoMap[_mm_extract_epi16(xyII1, 4) + (_mm_extract_epi16(xyII1, 6) + 1) * aoMapWidth],
                        aoMap[_mm_extract_epi16(xyII2, 0) + (_mm_extract_epi16(xyII2, 2) + 1) * aoMapWidth],
                        aoMap[_mm_extract_epi16(xyII2, 4) + (_mm_extract_epi16(xyII2, 6) + 1) * aoMapWidth]);
                auto aoZ4 = _mm_setr_ps(
                        aoMap[_mm_extract_epi16(xyII1, 0) + 1 + (_mm_extract_epi16(xyII1, 2) + 1) * aoMapWidth],
                        aoMap[_mm_extract_epi16(xyII1, 4) + 1 + (_mm_extract_epi16(xyII1, 6) + 1) * aoMapWidth],
                        aoMap[_mm_extract_epi16(xyII2, 0) + 1 + (_mm_extract_epi16(xyII2, 2) + 1) * aoMapWidth],
                        aoMap[_mm_extract_epi16(xyII2, 4) + 1 + (_mm_extract_epi16(xyII2, 6) + 1) * aoMapWidth]);

                aoZ1 = _mm_max_ps(aoZ1, minAoVolumeHeight);
                aoZ2 = _mm_max_ps(aoZ2, minAoVolumeHeight);
                aoZ3 = _mm_max_ps(aoZ3, minAoVolumeHeight);
                aoZ4 = _mm_max_ps(aoZ4, minAoVolumeHeight);

                // global coordinate's fracional part ([0, 255])
                auto xyIF1 = _mm_sub_epi32(xyI1, _mm_slli_epi32(xyII1, 8));
                auto xyIF2 = _mm_sub_epi32(xyI2, _mm_slli_epi32(xyII2, 8));
                auto xyFF1 = _mm_cvtepi32_ps(xyIF1);
                auto xyFF2 = _mm_cvtepi32_ps(xyIF2);
                auto xxFF = _mm_shuffle_ps(xyFF1, xyFF2, _MM_SHUFFLE(2, 0, 2, 0));
                auto yyFF = _mm_shuffle_ps(xyFF1, xyFF2, _MM_SHUFFLE(3, 1, 3, 1));
                xxFF = _mm_mul_ps(xxFF, _mm_set1_ps(1.f / 256.f));
                yyFF = _mm_mul_ps(yyFF, _mm_set1_ps(1.f / 256.f));

                // linear interpolate AO map
                aoZ1 = _mm_add_ps(aoZ1, _mm_mul_ps(xxFF, _mm_sub_ps(aoZ2, aoZ1)));
                aoZ3 = _mm_add_ps(aoZ3, _mm_mul_ps(xxFF, _mm_sub_ps(aoZ4, aoZ3)));
                auto aoZ = _mm_add_ps(aoZ1, _mm_mul_ps(yyFF, _mm_sub_ps(aoZ3, aoZ1)));

                // take difference
                auto aoDepth = _mm_sub_ps(terrainZ, aoZ);
                //aoDepth = _mm_min_ps(aoDepth, _mm_set1_ps(AORange));
                aoDepth = _mm_max_ps(aoDepth, _mm_setzero_ps());

                // approx exp(-aoDepth * hoge)
                auto aoDepthLog = _mm_cvttps_epi32(_mm_mul_ps(aoDepth, _mm_set1_ps(-(1<<22) * strength)));
                aoDepthLog = _mm_add_epi32(aoDepthLog, _mm_set1_epi32(0x3f800000));
                aoDepth = _mm_castsi128_ps(aoDepthLog);
                aoDepth = _mm_mul_ps(aoDepth, _mm_rsqrt_ps(aoDepth));
                //aoDepth = _mm_add_ps(_mm_mul_ps(_mm_sub_ps(_mm_set1_ps(1.f), aoDepth), _mm_set1_ps(0.6f)), aoDepth);

                // calculate color coef
                auto coeff = _mm_mul_ps(aoDepth, _mm_set1_ps(64.5f));   // f32 * 4
                /* DEBUG */ // coeff = _mm_mul_ps(aoZ, _mm_set1_ps(1.f ));
                auto coeffI = _mm_cvttps_epi32(coeff);                // i32 * 4
                coeffI = _mm_packs_epi32(coeffI, coeffI);          // i16 * 4 + pad
                coeffI = _mm_unpacklo_epi16(coeffI, coeffI);       // i16 [c1, c1, c2, c2, c3, c3, c4, c4]

                // load color
                auto col = _mm_loadu_si128(reinterpret_cast<__m128i*>(lineColorOutput));
                auto originalColor = col;
                auto colLo = _mm_unpacklo_epi8(col, _mm_setzero_si128());
                colLo = _mm_srai_epi16(_mm_mullo_epi16(colLo, _mm_unpacklo_epi16(coeffI, coeffI)), 6);
                auto colHi = _mm_unpackhi_epi8(col, _mm_setzero_si128());
                colHi = _mm_srai_epi16(_mm_mullo_epi16(colHi, _mm_unpackhi_epi16(coeffI, coeffI)), 6);
                col = _mm_packus_epi16(colLo, colHi);

                // keep background color original
                col = _mm_and_si128(_mm_castps_si128(isForeground), col);
                originalColor = _mm_andnot_si128(_mm_castps_si128(isForeground), originalColor);
                col = _mm_or_si128(col, originalColor);

                _mm_storeu_si128(reinterpret_cast<__m128i*>(lineColorOutput), col);

            }
        }
    });
}
