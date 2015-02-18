#ifndef DITHERSAMPLER_H
#define DITHERSAMPLER_H

#include <xmmintrin.h>

struct DitherSampler {
    float dither[65536];
    int pos = 0;
public:
    DitherSampler();

    inline float get() {
        pos = (pos + 4) & 4095;
        return dither[pos];
    }
    inline __m128 getM128() {
        pos = (pos + 4) & 4095;
        return _mm_loadu_ps(dither + pos);
    }
};
extern DitherSampler globalDitherSampler;

#endif // DITHERSAMPLER_H
