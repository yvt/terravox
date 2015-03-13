#ifndef CPU_H
#define CPU_H

#include <xmmintrin.h>

namespace CpuId
{
    enum class Feature
    {
        Mmx,
        Sse,
        Sse2,
        Sse3,
        Ssse3,
        Sse41,
        Sse42,
        Sse4a,
        Avx,
        F16c,
        Lzcnt,
        Popcnt,
        Prefetchw
    };

    template <class...T>
    bool supports(Feature feature, T...rest)
    {
        return supports(feature) && supports<>(rest...);
    }
    template<>
    bool supports(Feature feature);
}

class SseRoundingModeScope
{
    unsigned int oldMode;
public:
    SseRoundingModeScope(unsigned int newMode) :
        oldMode(_MM_GET_ROUNDING_MODE())
    {
        _MM_SET_ROUNDING_MODE(newMode);
    }
    ~SseRoundingModeScope()
    {
        _MM_SET_ROUNDING_MODE(oldMode);
    }
};

#endif // CPU_H

