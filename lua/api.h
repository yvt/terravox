#pragma once

#include <stdint.h>

typedef void *TerrainHandle;

struct TerravoxApi
{
    void (*aboutQt)(); // just for testing...

    TerrainHandle (*terrainCreate)(int width, int height);
    float *(*terrainGetLandformData)(TerrainHandle);
    uint32_t *(*terrainGetColorData)(TerrainHandle);
    void (*terrainGetSize)(TerrainHandle, int *outDimensions);
    void (*terrainRelease)(TerrainHandle);
};

