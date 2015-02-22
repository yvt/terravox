#pragma once

#include <stdint.h>

// rules for handle ownership can be described with the following C++ code:
//   Handle func(const Handle &);
// that is,
//   * returned handle must be released.
//   * handles passed by argument must NOT be released.
//     passing handle doesn't give the ownership to the callee.

typedef void *Host;

typedef void *TerrainHandle;
typedef void *TerrainEditHandle;

typedef void *ToolEditorHandle;

struct TerravoxEffectApi
{
    void *state;

    void (*preview)(void *);
};

struct TerravoxEffectController
{
    ToolEditorHandle editor; // ownership is not moved.

    // TODO: view

    void (*apply)(TerrainEditHandle);

    void (*destroy)();
};

typedef void (*TerravoxEffectControllerFactory)(struct TerravoxEffectApi *, struct TerravoxEffectController *outController);

typedef void (*ToolEditorControlEventHandler)(int controlId);

struct TerravoxApi
{
    Host host;

    void (*aboutQt)(); // just for testing...

    TerrainHandle (*terrainCreate)(int width, int height);
    float *(*terrainGetLandformData)(TerrainHandle);
    uint32_t *(*terrainGetColorData)(TerrainHandle);
    void (*terrainGetSize)(TerrainHandle, int *outDimensions);
    TerrainHandle (*terrainClone)(TerrainHandle);
    void (*terrainQuantize)(TerrainHandle);
    void (*terrainRelease)(TerrainHandle);

    TerrainHandle (*terrainEditGetTerrain)(TerrainEditHandle);
    void (*terrainEditBeginEdit)(TerrainEditHandle, int x, int y, int width, int height);
    void (*terrainEditEndEdit)(TerrainEditHandle);
    void (*terrainEditRelease)(TerrainEditHandle);

    ToolEditorHandle (*toolEditorCreate)(Host);
    void (*toolEditorRelease)(ToolEditorHandle);
    void (*toolEditorAddFinalizer)(ToolEditorHandle, void(*finalizer)());
    int (*toolEditorAddIntegerSlider)(ToolEditorHandle, ToolEditorControlEventHandler, const char *text,
                                      int minValue, int maxValue, int logarithmic);
    int (*toolEditorAddRealSlider)(ToolEditorHandle, ToolEditorControlEventHandler, const char *text,
                                   double minValue, double maxValue, int digits, int logarithmic);
    int (*toolEditorAddComboBox)(ToolEditorHandle, ToolEditorControlEventHandler, const char *text,
                                 const char *items);
    void (*toolEditorSetValue)(ToolEditorHandle, int controlId, double value);
    double (*toolEditorGetValue)(ToolEditorHandle, int controlId);

    void (*registerEffect)(Host host, const char *name, TerravoxEffectControllerFactory factory);
};

