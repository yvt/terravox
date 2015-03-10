#include "terraingenerator.h"
#include <memory>
#include "terrain.h"
#include <random>
#include <cstdint>

TerrainGenerator::TerrainGenerator(const QSize &size) :
    size_(size)
{

}

TerrainGenerator::~TerrainGenerator()
{

}

Terrain *TerrainGenerator::generateRandomLandform()
{
    std::unique_ptr<Terrain> terrain(new Terrain(size_));

    // Make sure size is power of two
    Q_ASSERT((size_.width() & (size_.width() - 1)) == 0);
    Q_ASSERT((size_.height() & (size_.height() - 1)) == 0);

    // Make sure size is square
    Q_ASSERT(size_.width() == size_.height());

    const int sz = size_.width();
    std::mt19937 rnd(static_cast<std::uint_fast32_t>(std::random_device()()));
    std::normal_distribution<float> dist;

    auto landform = [&](int x, int y) -> float& {
        return terrain->landform(x & sz - 1, y & sz - 1);
    };

    landform(0, 0) = 50.f;

    float noiseScale = 30.f;
    for(int detail = sz >> 1; detail >= 1; detail >>= 1) {
        int step = detail << 1;
        for (int y = 0; y < sz; y += step) {
            for (int x = 0; x < sz; x += step) {
                auto p1 = landform(x, y);
                auto p2 = landform(x + step, y);
                auto p3 = landform(x, y + step);
                auto p4 = landform(x + step, y + step);
                landform(x + detail, y) = (p1 + p2) * 0.5f + dist(rnd) * noiseScale;
                landform(x, y + detail) = (p1 + p3) * 0.5f + dist(rnd) * noiseScale;
                landform(x + detail, y + detail) = (p1 + p4 + p2 + p3) * 0.25f + dist(rnd) * noiseScale;
            }
        }
        noiseScale *= 0.5f;
    }

    for (int y = 0; y < sz; ++y) {
        for (int x = 0; x < sz; ++x) {
            float altitude = 62.5f - terrain->landform(x, y);
            float red, green, blue;

            float landAltitude = std::max(altitude, 0.f);

            red = landAltitude * 1.5f + 140.f;
            green = 160.f + std::max(landAltitude - 40.f, 0.f);
            blue =  80.f + std::max(landAltitude - 40.f, 0.f) * 2.f;

            if (altitude < 0.f) {
                float mix = 1.f - std::exp2(altitude * .3f);
                mix = .2f + mix * 0.8f;
                red += (10.f - red) * mix;
                green += (100.f - green) * mix;
                blue += (160.f - blue) * mix;
            }

            red = std::min(red, 255.f);
            green = std::min(green, 255.f);
            blue = std::min(blue, 255.f);
            red = std::max(red, 0.f);
            green = std::max(green, 0.f);
            blue = std::max(blue, 0.f);

            //red = green = blue = latitude * 4.f + 128.f;

            terrain->color(x, y) =
               static_cast<int>(blue) | (static_cast<int>(green) << 8) |
               (static_cast<int>(red) << 16);
        }
    }

    return terrain.release();
}

