
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
    q_ptr(e)
{
}

void LuaEnginePrivate::initialize()
{
    Q_Q(LuaEngine);
    lua = lua_open();
    if (!lua) {
        emit q->error(tr("Fail to start LuaJIT."));
        return;
    }
    luaL_openlibs(lua);

    // add searcher
    lua_pushcfunction(lua, &QtResourceSearcher);
    lua_setglobal(lua, "__qsearcher");

    api = createApiInterface();
    lua_pushinteger(lua, reinterpret_cast<lua_Integer>(&api));
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

LuaEnginePrivate::~LuaEnginePrivate()
{
    // lua_close(lua); // crashes
}

#endif

