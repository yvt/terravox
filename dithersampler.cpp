#include "dithersampler.h"
#include <random>

DitherSampler globalDitherSampler;

DitherSampler::DitherSampler() {
    std::random_device device;
    std::uniform_real_distribution<float> dist(-0.5f, 0.5f);
    for (auto &e: dither) {
        e = dist(device);
    }
}
