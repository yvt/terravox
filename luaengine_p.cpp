
#ifdef HAS_LUAJIT

#include "luaengine_p.h"
#include <QFile>
#include <QByteArray>
#include <QDebug>
#include <QApplication>
#include <QSharedPointer>
#include "luaapi.h"

static int QtResourceSearcher(lua_State *lua)
{
    // check module name prefix
    QString moduleName(luaL_checkstring(lua, 1));
    if (moduleName == "lclass") {
        // lclass is an external library, so no prefix needed
        moduleName.prepend("terravox.");
    }
    if (!moduleName.startsWith("terravox.")) {
        lua_pushnil(lua);
        return 1;
    }

    bool isApiFile = moduleName == "terravox.api";

    // try loading corresponding file
    QString path = moduleName;
    path.replace('.', '/');
    path += isApiFile ? ".h" : ".lua";
    path = ":/TerravoxLua/" + path.mid(9);
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        lua_pushstring(lua, (path + ": " + file.errorString()).toUtf8().data());
        return 1;
    }

    QByteArray source = file.readAll();
    if (isApiFile) {
        // translate to LuaJIT FFI definition code
        QString str(source);
        auto lines = str.split('\n');
        lines = lines.filter(QRegExp("^[^#].*$"));
        str = lines.join('\n');
        source = str.toUtf8();

        source.prepend("require(\"ffi\").cdef[[");
        source.append("]]");
    }
    luaL_loadbuffer(lua, source.data(), source.size(), moduleName.toUtf8().data());

    return 1;
}

LuaEnginePrivate::LuaEnginePrivate(LuaEngine *e) :
    q_ptr(e),
    isDestroyed(new bool(false))
{
}

void LuaEnginePrivate::initialize(LuaInterface *i)
{
    Q_Q(LuaEngine);
    lua = lua_open();
    if (!lua) {
        emit q->error(tr("Fail to start LuaJIT."));
        return;
    }

    static LuaEnginePrivate *priv = this; // quick hack (assuming LuaEnginePrivate is singleton)
    lua_atpanic(lua, [](lua_State *lua) {
        const char *msg = luaL_checkstring(lua, -1);
        qDebug() << msg;
        emit priv->q_func()->error(msg);
        return 0;
    });

    luaL_openlibs(lua);

    // add searcher
    lua_pushcfunction(lua, &QtResourceSearcher);
    lua_setglobal(lua, "__qsearcher");

    auto isDestroyed = this->isDestroyed;
    sinf.reset(new LuaScriptInterface(i, [=](std::function<void()> f){
        if (*isDestroyed) { // always fail the call after Lua engine is destroyed
            qDebug() << "FIXME: Lua code was called after Lua engine was destroyed.";
            return false;
        }
        return callProtected(f);
    }));
    lua_pushinteger(lua, reinterpret_cast<lua_Integer>(sinf->api()));
    lua_setglobal(lua, "__terravoxApi");

    // load main script
    QFile mainFile(":/TerravoxLua/main.lua");
    if (!mainFile.open(QIODevice::ReadOnly)) {
        emit q->error(tr("Fail to start LuaJIT main script."));
        return;
    }

    QByteArray mainSource = mainFile.readAll();
    Q_ASSERT(mainSource.length() > 0);
    int ret = luaL_loadbuffer(lua, mainSource.data(), mainSource.size(), "terravox.main");
    if (ret) {
        const char *msg = luaL_checkstring(lua, -1);
        emit q->error(tr("Fail to start LuaJIT main script.") + "\n\n" +
                      (msg ? QString(msg) : tr("(no information available)")));
        return;
    }
    if (!callProtected(0, 0)) {
        return;
    }

}

bool LuaEnginePrivate::callProtected(int nargs, int nresults)
{
    Q_Q(LuaEngine);

    int ret = lua_pcall(lua, nargs, nresults, 0);
    if (ret) {
        // fail
        const char *msg = luaL_checkstring(lua, -1);
        emit q->error(msg ? msg : tr("(unknown)"));
        return false;
    }
    return true;
}

bool LuaEnginePrivate::callProtected(std::function<void ()> fn)
{
    Q_Q(LuaEngine);

    int ret = lua_cpcall(lua, [](lua_State *lua) {
        // careful not to use RAII here because Lua might do longjmp...
        auto &fn = *reinterpret_cast<std::function<void ()> *>(lua_touserdata(lua, -1));
        lua_pop(lua, 1);
        fn();
        return 0;
    }, reinterpret_cast<void *>(&fn));
    if (ret) {
        // fail
        const char *msg = luaL_checkstring(lua, -1);
        emit q->error(msg ? msg : tr("(unknown)"));
        return false;
    }
    return true;
}

LuaEnginePrivate::~LuaEnginePrivate()
{
    *isDestroyed = true;
    // lua_close(lua); // crashes...
}

#endif

