#include "coherentnoisegenerator.h"
#include <random>
#include "temporarybuffer.h"

/*
 coef table
     128                                  0
 r1 = [  1       x       x^2     x^3      ]
 r2 = [  y       xy      x^2y    x^3 y    ]
 r3 = [  y^2     xy^2    x^2 y^2 x^3 y^2  ]
 r4 = [  y^3     xy^3    x^2 y^3 x^3 y^3  ]

 variablegen: r2 = y * r1, r3 = y * r2, r4 = y * r3
       ...or: r2 = y * r1, r3 = y^2 * r1, r4 = y^2 * r2

 modified coef table having x^4, y^4
     128                                  0
 r1 = [  1       x       x^2     x^3      ]
 r2 = [  y       xy      x^2y    x^3 y    ]
 r3 = [  y^2     xy^2    x^2 y^2 x^4      ]
 r4 = [  y^3     xy^3    --      y^4      ]

 gen: r2 = y * r1, r3 = {y^2|x} * r1, r4 = {y^2 * r2 | r4[y^3] * y}
  */

CoherentNoiseGenerator::CoherentNoiseGenerator(uint sizeBits) :
    size(1U << sizeBits),
    bits(sizeBits)
{
    const uint numCells = 1U << (bits << 1);
    coefTable.resize(numCells * 16);
}

CoherentNoiseGenerator::CoherentNoiseGenerator(uint sizeBits, std::uint_fast32_t seed) :
    CoherentNoiseGenerator(sizeBits)
{
    randomize(seed);
}

CoherentNoiseGenerator::~CoherentNoiseGenerator()
{

}

void CoherentNoiseGenerator::randomize(std::uint_fast32_t seed)
{
    std::mt19937 rnd(seed);
    std::uniform_real_distribution<float> fDist(-1.f, 1.f); // distribution for f(x, y)
    std::uniform_real_distribution<float> dDist(-1.f, 1.f); //                  grad f(x, y)

    const auto size = this->size;
    const auto bits = this->bits;
    const uint numCells = 1U << (bits << 1);
    TemporaryBuffer<float> fBuf (numCells);
    TemporaryBuffer<float> dxBuf(numCells);
    TemporaryBuffer<float> dyBuf(numCells);
    auto F =  [&](uint x, uint y) -> float&
    { return fBuf[(x & size - 1) + ((y & size - 1) << bits)]; };
    auto DX = [&](uint x, uint y) -> float&
    { return dxBuf[(x & size - 1) + ((y & size - 1) << bits)]; };
    auto DY = [&](uint x, uint y) -> float&
    { return dyBuf[(x & size - 1) + ((y & size - 1) << bits)]; };
    auto T = [](uint row, uint col) -> uint
    { return col + row * 4; };

    for (uint i = 0; i < numCells; ++i)
        fBuf[i] = 0.0f; //fDist(rnd);
    for (uint i = 0; i < numCells; ++i)
        dxBuf[i] = dDist(rnd);
    for (uint i = 0; i < numCells; ++i)
        dyBuf[i] = dDist(rnd);

    // create coef table
    for (uint y = 0; y < size; ++y) {
        for (uint x = 0; x < size; ++x) {
            auto c0 = F(x, y),     c1 = F(x + 1, y);
            auto c2 = F(x, y + 1), c3 = F(x + 1, y + 1);
            auto dx1 = DX(x, y),     dx2 = DX(x + 1, y);
            auto dx3 = DX(x, y + 1), dx4 = DX(x + 1, y + 1);
            auto dy1 = DY(x, y),     dy2 = DY(x + 1, y);
            auto dy3 = DY(x, y + 1), dy4 = DY(x + 1, y + 1);
            float *outCoef = coefTable.data() + (x + (y << bits)) * 16;
            /*        0                                    128
             *   r1 = [  a       x0      x1     x2      ]
                 r2 = [  y0      xy0     xy2    xy3    ]
                 r3 = [  y1      xy1     --    --      ]
                 r4 = [  y2      xy5    --      --        ]
            */

            outCoef[T(0, 0)] = c0;
            outCoef[T(0, 1)] = dx1;
            outCoef[T(0, 2)] = 3*(c1 - c0) - 2*dx1 - dx2;
            outCoef[T(0, 3)] = dx1 + dx2 - 2*(c1 - c0);
            outCoef[T(1, 0)] = dy1;
            outCoef[T(1, 1)] = (c1 - c0) + (c2 - c3) - dx1 + dx3 - dy1 + dy2;
            outCoef[T(1, 2)] = 2*(dx1 - dx3) + (dx2 - dx4) - 3*((c2-c3) + (c1-c0));
            outCoef[T(1, 3)] = 2*((c1 - c0) + (c2 - c3)) + dx3 + dx4 - dx1 - dx2;
            outCoef[T(2, 0)] = 3*(c2 - c0) - 2*dy1 - dy3;
            outCoef[T(2, 1)] = 2*(dy1 - dy2) + (dy3 - dy4) - 3*((c2-c3) + (c1-c0));
            outCoef[T(2, 2)] = 0.f;
            outCoef[T(2, 3)] = 0.f;
            outCoef[T(3, 0)] = dy1 + dy3 - 2*(c2 - c0);
            outCoef[T(3, 1)] = 2*((c1 - c0) + (c2 - c3)) + dy2 + dy4 - dy1 - dy3;
            outCoef[T(3, 2)] = 0.f;
            outCoef[T(3, 3)] = 0.f;

        }
    }


}

