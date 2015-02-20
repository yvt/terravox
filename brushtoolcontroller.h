#ifndef BRUSHTOOLCONTROLLER_H
#define BRUSHTOOLCONTROLLER_H

#include <QObject>
#include "toolcontroller.h"
#include <QSharedPointer>
#include "brushtool.h"

class BrushTool;

class BrushToolController : public ToolController
{
    Q_OBJECT
public:
    BrushToolController(BrushType);
    ~BrushToolController();

    QWidget *createEditor(Session *) override;
    ToolView *createView(Session *, TerrainView *) override;
    QString name() override;
private:
    QSharedPointer<BrushTool> tool;
    QString settingsGroupName();
};

#endif // BRUSHTOOLCONTROLLER_H
