#ifndef LUAENGINE_H
#define LUAENGINE_H

#include <QObject>
#include <QString>

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

    void initialize();

    QString pluginDirectory();
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

    void initialize() { }

    QString pluginDirectory() { return QString(); }
signals:
    void error(QString);

public slots:
};

#endif

#endif // LUAENGINE_H
