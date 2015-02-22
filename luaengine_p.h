#ifndef LUAENGINEPRIVATE_H
#define LUAENGINEPRIVATE_H

#include "luaengine.h"
#include <functional>
#include <QScopedPointer>
#include <QSharedPointer>

#ifdef HAS_LUAJIT

#include "lua/api.h"
class LuaScriptInterface;

class LuaEnginePrivate : public QObject
{
    Q_OBJECT
    Q_DECLARE_PUBLIC(LuaEngine)
    LuaEngine *q_ptr;

    lua_State *lua;

    QScopedPointer<LuaScriptInterface> sinf;
    QSharedPointer<bool> isDestroyed;

    bool callProtected(int nargs, int nresults);
    bool callProtected(std::function<void()>);

public:
    LuaEnginePrivate(LuaEngine *e);

    void initialize(LuaInterface *i);

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
