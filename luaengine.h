#ifndef LUAENGINE_H
#define LUAENGINE_H

#include <QObject>
#include <QString>
#include <functional>
#include <QStringList>

class EffectController;

class LuaInterface
{
public:
    virtual ~LuaInterface() {}
    virtual void registerEffect(const QString &name, std::function<EffectController *()>) = 0;
};

#ifdef HAS_LUAJIT

#include <lua.hpp>

class LuaEnginePrivate;

class LuaEngine : public QObject
{
    Q_OBJECT

    LuaEnginePrivate *d_ptr;
    Q_DECLARE_PRIVATE(LuaEngine)
public:
    explicit LuaEngine(QObject *parent = 0);
    ~LuaEngine();

    void initialize(LuaInterface *i);

    // FIXME: move plugin-related stuffs to a separate module (like PluginManager)
    QStringList pluginDirectories(bool writable);
signals:
    void error(QString);

public slots:
};

#else

class LuaEngine : public QObject
{
    Q_OBJECT

public:
    explicit LuaEngine(QObject *parent = 0);
    ~LuaEngine();

    void initialize(LuaInterface *);

    QStringList pluginDirectories(bool writable) { return QStringList(); }
signals:
    void error(QString);

public slots:
};

#endif

#endif // LUAENGINE_H
