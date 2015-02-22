#include "luaapi.h"
#include "terrain.h"
#include <QSharedPointer>
#include <QApplication>

static TerrainHandle createHandle(QSharedPointer<Terrain> t)
{
    return reinterpret_cast<TerrainHandle>(new QSharedPointer<Terrain>(t));
}
static QSharedPointer<Terrain> *fromHandle(TerrainHandle h)
{
    return reinterpret_cast<QSharedPointer<Terrain> *>(h);
}

TerravoxApi createApiInterface()
{
    TerravoxApi api;

    api.aboutQt = []() {
        QApplication::aboutQt();
    };

    // Terrain API
    api.terrainCreate = [](int width, int height) -> TerrainHandle {
        auto t = QSharedPointer<Terrain>::create(QSize(width, height));
        return createHandle(t);
    };
    api.terrainRelease = [](TerrainHandle h) -> void {
        delete fromHandle(h);
    };
    api.terrainGetColorData = [](TerrainHandle h) -> uint32_t* {
        return (*fromHandle(h))->colorData();
    };
    api.terrainGetLandformData = [](TerrainHandle h) -> float* {
        return (*fromHandle(h))->landformData();
    };
    api.terrainGetSize = [](TerrainHandle h, int *outDim) -> void {
        auto &t = (*fromHandle(h));
        outDim[0] = t->size().width();
        outDim[1] = t->size().height();
    };

    return api;
}
