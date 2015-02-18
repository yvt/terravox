#ifndef TERRAINGENERATOR_H
#define TERRAINGENERATOR_H

#include <QSize>

class Terrain;

class TerrainGenerator
{
public:
    TerrainGenerator(const QSize &size);
    ~TerrainGenerator();

    Terrain *generateRandomLandform();

private:
    QSize size_;
};

#endif // TERRAINGENERATOR_H
