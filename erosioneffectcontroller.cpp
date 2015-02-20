#include "erosioneffectcontroller.h"
#include "erosioneffect.h"
#include "erosioneditor.h"
#include <QSettings>

ErosionEffectController::ErosionEffectController(QObject *parent) :
    EffectController(parent)
{
    fx.reset(new ErosionEffect());

    QSettings settings;
    settings.beginGroup("erosion");

    auto params = fx->parameters();
    params.density = settings.value("density", params.density).value<float>();
    params.strength = settings.value("strength", params.strength).value<float>();
    fx->setParameters(params);

    settings.endGroup();

    connect(fx.data(), SIGNAL(parameterChanged(ErosionParameters)), SLOT(preview()));
}

ErosionEffectController::~ErosionEffectController()
{
    QSettings settings;
    settings.beginGroup("erosion");

    auto params = fx->parameters();
    settings.setValue("density", params.density);
    settings.setValue("strength", params.strength);

    settings.endGroup();
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

