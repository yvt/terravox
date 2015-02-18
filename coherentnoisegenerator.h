#ifndef COHERENTNOISEGENERATOR_H
#define COHERENTNOISEGENERATOR_H

#include <cstdint>
#include <QVector>
#include <xmmintrin.h>
#include <smmintrin.h>

class CoherentNoiseGenerator
{
    uint size;
    uint bits;
    QVector<float> coefTable;

public:
    CoherentNoiseGenerator(uint sizeBits);
    CoherentNoiseGenerator(uint sizeBits, std::uint_fast32_t);
    ~CoherentNoiseGenerator();

    void randomize(std::uint_fast32_t seed);

    class Sampler
    {
        friend class CoherentNoiseGenerator;

        uint size;
        uint bits;
        float *coefs;
        __m128i mask;
        Sampler(CoherentNoiseGenerator &gen) :
            size(gen.size), bits(gen.bits), coefs(gen.coefTable.data())
        {
            mask = _mm_set1_epi32(size - 1);
        }
    public:
        inline float sample(__m128 coord /* [ x, y, [who cares?]x2 ] from lower to upper */) {
            // compute index
            auto coordIF = _mm_floor_ps(coord);
            auto coordI = _mm_cvtps_epi32(coordIF);
            coordI = _mm_and_si128(coordI, mask);

            uint index = _mm_extract_epi16(coordI, 0);
            index |= _mm_extract_epi16(coordI, 2) << bits;

            float *coefptr = coefs + index * 16;

            // interpolate
            coord = _mm_sub_ps(coord, coordIF);
            auto coord2 = _mm_mul_ps(coord, coord); // [ xx, yy, ?, ? ]

            auto x1 = _mm_unpacklo_ps(_mm_set_ss(1.f), coord); // [1, x, ?, y]
            x1 = _mm_movelh_ps(x1, x1); // [1, x, 1, x]
            auto x2 = _mm_shuffle_ps(x1, coord2, _MM_SHUFFLE(0, 0, 0, 0)); // [1, 1, xx, xx]
            auto xx = _mm_mul_ps(x1, x2); // [1, x, xx, xxx]

            auto y = _mm_shuffle_ps(coord, coord, _MM_SHUFFLE(1, 1, 1, 1));
            auto yy = _mm_shuffle_ps(coord2, coord2, _MM_SHUFFLE(1, 1, 1, 1));

            // compute [1, x, x^2, x^3]
            auto r1 = xx;
            // compute ... [y^3, xy^3, x^2y^3, x^3y^3]
            auto r2 = _mm_mul_ps(r1, y);
            auto r3 = _mm_mul_ps(r1, yy);
            auto r4 = _mm_mul_ps(r2, yy);

            // multiply coefs
            r1 = _mm_mul_ps(r1, _mm_loadu_ps(coefptr));
            r2 = _mm_mul_ps(r2, _mm_loadu_ps(coefptr + 4));
            r3 = _mm_mul_ps(r3, _mm_loadu_ps(coefptr + 8));
            r4 = _mm_mul_ps(r4, _mm_loadu_ps(coefptr + 12));

            // take sum
            r1 = _mm_add_ps(r1, r2);
            r3 = _mm_add_ps(r3, r4);
            r1 = _mm_add_ps(r1, r3);
            r1 = _mm_hadd_ps(r1, r1);
            r1 = _mm_hadd_ps(r1, r1);

            float r;
            _mm_store_ss(&r, r1);
            return r;
        }
    };
    Sampler sampler() { return Sampler(*this); }
};

#endif // COHERENTNOISEGENERATOR_H
