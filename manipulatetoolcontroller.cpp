#include "manipulatetoolcontroller.h"
#include "manipulatetoolview.h"

ManipulateToolController::ManipulateToolController(ManipulateMode mode, QObject *parent) :
    ToolController(parent),
    mode_(mode)
{

}

ManipulateToolController::~ManipulateToolController()
{

}

QWidget *ManipulateToolController::createEditor(Session *s)
{
    return nullptr;
}

ToolView *ManipulateToolController::createView(Session *session, TerrainView *view)
{
    return new ManipulateToolView(mode_, session, view);
}

QString ManipulateToolController::name()
{
    switch (mode_) {
    case ManipulateMode::Landform:
        return "Manipulate";
    case ManipulateMode::Color:
        return "Paint";
    default:
        Q_UNREACHABLE();
        return QString();
    }
}

