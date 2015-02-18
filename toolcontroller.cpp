#include "toolcontroller.h"

ToolController::ToolController(QObject *parent) : QObject(parent)
{

}

ToolController::~ToolController()
{

}

bool ToolController::leave(std::function<void (bool)> callback)
{
    (void) callback;
    return true;
}

