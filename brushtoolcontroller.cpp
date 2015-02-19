#include "brushtoolcontroller.h"
#include "brusheditor.h"
#include "brushtoolview.h"
#include "brushtool.h"

BrushToolController::BrushToolController(BrushType type)
{
    tool = QSharedPointer<BrushTool>::create(type);
}

BrushToolController::~BrushToolController()
{

}

QWidget *BrushToolController::createEditor(Session *session)
{
    (void) session;
    return new BrushEditor(tool);
}

ToolView *BrushToolController::createView(Session *session, TerrainView *terrainView)
{
    return new BrushToolView(terrainView, session, tool.data());
}

QString BrushToolController::name()
{
    switch (tool->type()) {
    case BrushType::Raise: return "Raise";
    case BrushType::Lower: return "Lower";
    case BrushType::Paint: return "Paint Brush";
    case BrushType::Smoothen: return "Smoothen";
    case BrushType::Blur: return "Blur";
    }
}

