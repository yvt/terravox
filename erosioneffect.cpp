#include "erosioneffect.h"
#include "terrain.h"
#include "terrainedit.h"
#include "temporarybuffer.h"
#include <random>

ErosionEffect::ErosionEffect(QObject *parent) : QObject(parent)
{
    parameters_.density = 50.f;
    parameters_.strength = 50.f;
}

ErosionEffect::~ErosionEffect()
{

}

void ErosionEffect::apply(Terrain *terrain, TerrainEdit *edit)
{
    edit->beginEdit(QRect(QPoint(0, 0), terrain->size()), terrain);

    edit->copyBeforeTo(QRect(QPoint(0, 0), terrain->size()), terrain, terrain);

    int tWidth = terrain->size().width();
    int tHeight = terrain->size().height();
    Q_ASSUME(tWidth == 512); tWidth = 512;
    Q_ASSUME(tHeight == 512); tHeight = 512;

    int tArea = tWidth * tHeight;

    int numParticles = static_cast<int>(parameters_.density / 100.f * 14000.f);

    std::mt19937 rnd(332);
    std::uniform_real_distribution<float> dist(0.f, 1.f);

    TemporaryBuffer<float> px  (numParticles);  // momentum
    TemporaryBuffer<float> py  (numParticles);  // momentum
    TemporaryBuffer<float> vx  (numParticles);  // momentum
    TemporaryBuffer<float> vy  (numParticles);  // momentum
    TemporaryBuffer<float> amtS(numParticles); // mass of flowing soil
    float *landform = terrain->landformData();

    float massOfSoilPerHeight = 1.f;
    float heightPerMassOfSoil = 1.f / massOfSoilPerHeight;
    float precipitationRate = .02f;
    float excavationHeightPerVelocity = .5 * parameters_.strength / 100.f;
    float massOfWaterPerParticle = 1.f;
    float accelerationRate = .2f;
    float friction = .02f;
    float waterLevel = 63.f;

    auto I = [=](int x, int y) { return x + y * tWidth; };

    for (int i = 0; i < numParticles; ++i) {
        px[i] = dist(rnd) * tWidth;
        py[i] = dist(rnd) * tHeight;
        vx[i] = 0;
        vy[i] = 0;
        amtS[i] = 0.f;
    }

    for (int step = 0; step < 256; ++step) {
        for (int i = 0; i < numParticles; ++i) {
            int ix = static_cast<int>(px[i]);
            int iy = static_cast<int>(py[i]);
            if (ix < 0 || iy < 0 || ix >= tWidth || iy >= tHeight) {
                // went beyond limit

            } else {
                float& lf = landform[I(ix, iy)];
                float& aS = amtS[i];

                if (lf >= waterLevel - 0.5f || ((i & 15) == 0)) {
                    if (lf < waterLevel - 0.5f)
                        lf -= aS * heightPerMassOfSoil;

                    // reemit particle
                    px[i] = dist(rnd) * tWidth;
                    py[i] = dist(rnd) * tHeight;
                    vx[i] = 0;
                    vy[i] = 0;
                    amtS[i] = 0.f;
                    continue;
                }


                // precipitation
                float decS = aS * precipitationRate;
                lf -= decS * heightPerMassOfSoil;
                aS -= decS;

                // check slope
                int offX = rand() & 1;
                int offY = rand() & 1;
                float sx = landform[I(std::min(ix + offX, tWidth - 1), iy)] - landform[I(std::max(ix + offX - 1, 0), iy)];
                float sy = landform[I(ix, std::min(iy + offX, tHeight - 1))] - landform[I(ix, std::max(0, iy + offY - 1))];

                // excavation
                float exc = std::sqrt(vx[i] * vx[i] + vy[i] * vy[i]) * excavationHeightPerVelocity;
                exc = std::min(exc, waterLevel - lf);
                lf += exc;
                aS += exc * massOfSoilPerHeight;

                // acceleration
                vx[i] += sx * accelerationRate;
                vy[i] += sy * accelerationRate;

                // friction
                //vx[i] *= 1.f - friction;
                //vy[i] *= 1.f - friction;

                float sq = vx[i] * vx[i] + vy[i] * vy[i];
                if (sq > 1.f) {
                    sq = 1.f / std::sqrt(sq);
                    vx[i] *= sq; vy[i] *= sq;
                }

            }
            px[i] += vx[i];
            py[i] += vy[i];
        }

    }

    terrain->quantize();

    edit->endEdit(terrain);
}

void ErosionEffect::setParameters(const ErosionParameters &p)
{
    parameters_ = p;
    emit parameterChanged(p);
}

