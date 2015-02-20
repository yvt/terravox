#include "brushtoolcontroller.h"
#include "brusheditor.h"
#include "brushtoolview.h"
#include "brushtool.h"
#include <QSettings>
#include <QDataStream>

QDataStream &operator<<(QDataStream &out, const BrushTipType &t)
{
    QString name;
    switch (t) {
    case BrushTipType::Bell: name = "bell"; break;
    case BrushTipType::Cone: name = "cone"; break;
    case BrushTipType::Cylinder: name = "cylinder"; break;
    case BrushTipType::Mountains: name = "mountains"; break;
    case BrushTipType::Sphere: name = "sphere"; break;
    case BrushTipType::Square: name = "square"; break;
    }
    return out << name;
}

QDataStream &operator>>(QDataStream &in, BrushTipType &t)
{
    QString name;
    in >> name;
    if (name == "bell") {
        t = BrushTipType::Bell;
    } else if (name == "bell") {
        t = BrushTipType::Bell;
    } else if (name == "cone") {
        t = BrushTipType::Cone;
    } else if (name == "cylinder") {
        t = BrushTipType::Cylinder;
    } else if (name == "mountains") {
        t = BrushTipType::Mountains;
    } else if (name == "sphere") {
        t = BrushTipType::Sphere;
    } else if (name == "square") {
        t = BrushTipType::Square;
    }
    return in;
}

QDataStream &operator<<(QDataStream &out, const BrushPressureMode &t)
{
    QString name;
    switch (t) {
    case BrushPressureMode::Adjustable: name = "adjustable"; break;
    case BrushPressureMode::AirBrush: name = "airbrush"; break;
    case BrushPressureMode::Constant: name = "constant"; break;
    }
    return out << name;
}

QDataStream &operator>>(QDataStream &in, BrushPressureMode &t)
{
    QString name;
    in >> name;
    if (name == "adjustable") {
        t = BrushPressureMode::Adjustable;
    } else if (name == "airbrush") {
        t = BrushPressureMode::AirBrush;
    } else if (name == "constant") {
        t = BrushPressureMode::Constant;
    }
    return in;
}


BrushToolController::BrushToolController(BrushType type)
{
    qRegisterMetaTypeStreamOperators<BrushTipType>("BrushTipType");
    qRegisterMetaTypeStreamOperators<BrushPressureMode>("BrushPressureMode");

    tool = QSharedPointer<BrushTool>::create(type);

    QSettings settings;
    settings.beginGroup(settingsGroupName());

    auto params = tool->parameters();
    params.tipType = settings.value("tip", QVariant::fromValue(params.tipType)).value<BrushTipType>();
    params.strength = settings.value("strength", params.strength).value<float>();
    params.size = settings.value("size", params.size).value<int>();
    params.scale = settings.value("scale", params.scale).value<float>();
    params.color = settings.value("color", params.color).value<quint32>();
    params.pressureMode = settings.value("pressureMode", QVariant::fromValue(params.pressureMode)).value<BrushPressureMode>();
    tool->setParameters(params);

    settings.endGroup();
}

BrushToolController::~BrushToolController()
{
    QSettings settings;
    settings.beginGroup(settingsGroupName());

    auto params = tool->parameters();
    settings.setValue("tip", QVariant::fromValue(params.tipType));
    settings.setValue("strength", params.strength);
    settings.setValue("size", params.size);
    settings.setValue("scale", params.scale);
    settings.setValue("color", params.color);
    settings.setValue("pressureMode", QVariant::fromValue(params.pressureMode));

    settings.endGroup();
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
    case BrushType::Raise: return tr("Raise");
    case BrushType::Lower: return tr("Lower");
    case BrushType::Paint: return tr("Paint Brush");
    case BrushType::Smoothen: return tr("Smoothen");
    case BrushType::Blur: return tr("Blur");
    default: Q_UNREACHABLE(); return QString();
    }
}

QString BrushToolController::settingsGroupName()
{
    switch (tool->type()) {
    case BrushType::Raise: return "raisetool";
    case BrushType::Lower: return "lowertool";
    case BrushType::Paint: return "paintbrushtool";
    case BrushType::Smoothen: return "smoothentool";
    case BrushType::Blur: return "bluetool";
    default: Q_UNREACHABLE(); return QString();
    }
}

