#include "cpu.h"
#include <cstdint>
#include <QObject>

struct CpuIdInfo
{
    std::uint32_t eax, ebx, ecx, edx;
};

#if defined(_MSC_VER)
#include <intrin.h>
static CpuIdInfo GetCpuId(std::uint32_t eax, std::uint32_t ecx)
{
    int info[4];
    CpuIdInfo retInfo;

    __cpuidex(info, eax, ecx);

    retInfo.eax = info[0];
    retInfo.ebx = info[1];
    retInfo.ecx = info[2];
    retInfo.edx = info[3];

    return retInfo;
}
#elif defined(__GNUC__)
static CpuIdInfo GetCpuId(std::uint32_t eax, std::uint32_t ecx)
{
    CpuIdInfo info;
    asm volatile
    (
        "cpuid\n\t"
        "mov %%eax, (%0)\n\t"
        "mov %%ebx, 4(%0)\n\t"
        "mov %%ecx, 8(%0)\n\t"
        "mov %%edx, 12(%0)\n\t"
        : /* no output registers */
        : "r"(&info), "a"(eax), "c"(ecx)
        : "%eax", "%ebx", "%ecx", "%edx"
    );
    return info;
}
#else
#error Cannot detect compiler
#endif

static CpuIdInfo cpuFeatureInfo = GetCpuId(1, 0);
static CpuIdInfo cpuExtendedFeatureInfo = GetCpuId(0x80000001, 0);

namespace CpuId
{
    template<>
    bool supports(Feature feature)
    {
        switch (feature)
        {
        case Feature::Mmx:
            return cpuFeatureInfo.edx & (1U << 23);
        case Feature::Sse:
            return cpuFeatureInfo.edx & (1U << 25);
        case Feature::Sse2:
            return cpuFeatureInfo.edx & (1U << 26);
        case Feature::Sse3:
            return cpuFeatureInfo.ecx & (1U << 0);
        case Feature::Ssse3:
            return cpuFeatureInfo.ecx & (1U << 9);
        case Feature::Sse41:
            return cpuFeatureInfo.ecx & (1U << 19);
        case Feature::Sse42:
            return cpuFeatureInfo.ecx & (1U << 20);
        case Feature::Sse4a:
            return cpuExtendedFeatureInfo.ecx & (1U << 6);
        case Feature::Avx:
            return cpuFeatureInfo.ecx & (1U << 28);
        case Feature::Lzcnt:
            return cpuExtendedFeatureInfo.ecx & (1U << 5);
        case Feature::Popcnt:
            return cpuFeatureInfo.ecx & (1U << 23);
        case Feature::Prefetchw:
            return cpuExtendedFeatureInfo.ecx & (1U << 8);
        case Feature::F16c:
            return cpuFeatureInfo.ecx & (1U << 29);
        }
        Q_UNREACHABLE();
    }
}

