#include "luaengine.h"

#include "luaengine_p.h"

#ifdef HAS_LUAJIT


LuaEngine::LuaEngine(QObject *parent) :
    QObject(parent),
    d_ptr(new LuaEnginePrivate(this))
{

}

LuaEngine::~LuaEngine()
{
    delete d_ptr;
}

void LuaEngine::initialize(LuaInterface *i)
{
    Q_D(LuaEngine);
    d->initialize(i);
}

#else

LuaEngine::LuaEngine(QObject *parent) :
    QObject(parent)
{

}

LuaEngine::~LuaEngine()
{
}


#endif
