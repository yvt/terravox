#ifndef TOOLCONTROLLER_H
#define TOOLCONTROLLER_H

#include <QObject>
#include <QSharedPointer>
#include <functional>

class QWidget;
class ToolEditor;
class ToolView;
class Session;
class TerrainView;

class ToolView : public QObject
{
    Q_OBJECT
protected:
    ToolView() { }
public:
    virtual ~ToolView() { }
};

class ToolController : public QObject
{
    Q_OBJECT
public:
    explicit ToolController(QObject *parent = 0);
    ~ToolController();

    virtual QWidget *createEditor(Session *) = 0;
    virtual ToolView *createView(Session *, TerrainView *) { return nullptr; }
    /** true = okay to leave, false = callback() will be called later */
    virtual bool leave(std::function<void(bool)> callback);
    virtual QString name() = 0;

signals:

public slots:
};

#endif // TOOLCONTROLLER_H
