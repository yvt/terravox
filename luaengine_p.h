#ifndef LUAENGINEPRIVATE_H
#define LUAENGINEPRIVATE_H

#include "luaengine.h"

#ifdef HAS_LUAJIT

#include "lua/api.h"

class LuaEnginePrivate : public QObject
{
    Q_OBJECT
    Q_DECLARE_PUBLIC(LuaEngine)
    LuaEngine *q_ptr;

    lua_State *lua;

    TerravoxApi api;

    bool callProtected(int nargs, int nresults);

public:
    LuaEnginePrivate(LuaEngine *e);

    void initialize();

    ~LuaEnginePrivate();
};

#else
class LuaEnginePrivate : public QObject
{
    Q_OBJECT
public:
    LuaEnginePrivate(LuaEngine *e) { }
    ~LuaEnginePrivate() { }
};
#endif

#endif // LUAENGINEPRIVATE_H
