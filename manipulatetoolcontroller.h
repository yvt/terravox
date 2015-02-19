#ifndef MANIPULATETOOLCONTROLLER_H
#define MANIPULATETOOLCONTROLLER_H

#include "toolcontroller.h"

enum class ManipulateMode : int
{
    Landform,
    Color
};

class ManipulateToolController : public ToolController
{
    Q_OBJECT
public:
    ManipulateToolController(ManipulateMode mode, QObject *parent = 0);
    ~ManipulateToolController();

    QWidget *createEditor(Session *) Q_DECL_OVERRIDE;
    ToolView *createView(Session *, TerrainView *) Q_DECL_OVERRIDE;
    QString name() Q_DECL_OVERRIDE;

private:
    ManipulateMode mode_;
};

#endif // MANIPULATETOOLCONTROLLER_H
