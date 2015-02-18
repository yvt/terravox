#include "erosioneffectcontroller.h"
#include "erosioneffect.h"
#include "erosioneditor.h"

ErosionEffectController::ErosionEffectController(QObject *parent) :
    EffectController(parent)
{
    fx.reset(new ErosionEffect());

    connect(fx.data(), SIGNAL(parameterChanged(ErosionParameters)), SLOT(preview()));
}

ErosionEffectController::~ErosionEffectController()
{

}

QString ErosionEffectController::name()
{
    return "Erosion";
}

QWidget *ErosionEffectController::createEffectEditor(Session *)
{
    ErosionEditor *editor = new ErosionEditor(fx.data());
    return editor;
}

void ErosionEffectController::applyEffect(Terrain *terrain, QSharedPointer<TerrainEdit> edit, Session *)
{
    fx->apply(terrain, edit.data());
}

