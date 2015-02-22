#include "luaengine.h"

#include "luaengine_p.h"
#include <QApplication>
#include <QStandardPaths>
#include <QHash>

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

QStringList LuaEngine::pluginDirectories(bool writable)
{
    QStringList ret;
    QHash<QString, bool> included;

    if (writable) {
        QString path = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
        if (!path.isEmpty()) {
            QString p = path + "/Scripts";
            ret.push_back(p);
        }
    } else {
        foreach(const QString &path, QStandardPaths::standardLocations(QStandardPaths::AppLocalDataLocation))
        {
            QString p = path + "/Scripts";
            if (included.contains(p)) {
                continue;
            }
            ret.push_back(p);
            included[p] = true;
        }
    }

    return ret;
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
