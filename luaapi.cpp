#include "luaapi.h"
#include "terrain.h"
#include <QSharedPointer>
#include <QApplication>
#include "luaengine.h"
#include "terrainedit.h"
#include <QWidget>
#include <QHBoxLayout>
#include <QHash>
#include "labelslider.h"
#include <QSpinBox>
#include <QDoubleSpinBox>
#include "effectcontroller.h"
#include <QDebug>

struct ToolEditor
{
    struct Control
    {
        int id;
        virtual ~Control() {}
        virtual void setValue(double) {}
        virtual double getValue() { return 0.; }
    };
    class IntSlider : public Control
    {
        LabelSlider *label;
        QSpinBox *spinBox;
    public:
        IntSlider(QHBoxLayout *layout, QString text, int min, int max, bool log,
                  std::function<void(Control*)> onchange)
        {
            label = new LabelSlider();
            spinBox = new QSpinBox();

            label->setText(text + ":");
            label->bindSpinBox(spinBox);
            spinBox->setRange(min, max);

            if (log)
                label->setLogarithmic();

            layout->addWidget(label);
            layout->addWidget(spinBox);

            spinBox->connect(spinBox, (void(QSpinBox::*)(int))&QSpinBox::valueChanged, [=](int) {
                onchange(this);
            });
        }

        void setValue(double value) { spinBox->setValue(static_cast<int>(value)); }
        double getValue() { return spinBox->value(); }
    };
    class RealSlider : public Control
    {
        LabelSlider *label;
        QDoubleSpinBox *spinBox;
    public:
        RealSlider(QHBoxLayout *layout, QString text, double min, double max, int digits, bool log,
                  std::function<void(Control*)> onchange)
        {
            label = new LabelSlider();
            spinBox = new QDoubleSpinBox();

            label->setText(text + ":");
            label->bindSpinBox(spinBox);
            spinBox->setRange(min, max);
            spinBox->setDecimals(digits);

            if (log)
                label->setLogarithmic();

            layout->addWidget(label);
            layout->addWidget(spinBox);

            spinBox->connect(spinBox, (void(QDoubleSpinBox::*)(double))&QDoubleSpinBox::valueChanged, [=](double) {
                onchange(this);
            });
        }

        void setValue(double value) { spinBox->setValue(value);}
        double getValue() { return spinBox->value(); }
    };

    QWidget *widget;
    QHBoxLayout *layout;
    QHash<int, QSharedPointer<Control>> controls;
    int nextControlId = 1;
    LuaHost &host;
    std::vector<std::function<void()>> finalizers;

    ToolEditor(LuaHost &host) :
        widget(new QWidget()),
        layout(new QHBoxLayout()),
        host(host)
    {
        layout->setMargin(0);
        widget->setLayout(layout);
    }

    int addIntSlider(std::function<void(Control*)> handler, QString label,
                     int minValue, int maxValue, bool logarithmic)
    {
        int id = nextControlId++;
        QSharedPointer<IntSlider> ctrl = QSharedPointer<IntSlider>::create
                (layout, label, minValue, maxValue, logarithmic,
                 handler);
        ctrl->id = id;
        controls[id] = ctrl;
        return id;
    }
    int addRealSlider(std::function<void(Control*)> handler, QString label,
                     double minValue, double maxValue, int digits, bool logarithmic)
    {
        int id = nextControlId++;
        QSharedPointer<RealSlider> ctrl = QSharedPointer<RealSlider>::create
                (layout, label, minValue, maxValue, digits, logarithmic,
                 handler);
        ctrl->id = id;
        controls[id] = ctrl;
        return id;
    }

    Control *controlById(int id)
    {
        return controls.value(id).data();
    }

    QWidget *takeWidget()
    {
        Q_ASSERT(widget);
        QWidget *w = widget;
        widget = nullptr;
        return w;
    }

    ~ToolEditor()
    {
        host.pcall([=]{
            for (const auto &f: finalizers)
                f();
        });
        if (widget)
            widget->deleteLater();
        qDebug() << "delete " << this;
    }
};

struct LuaTerrainEdit
{
    QSharedPointer<TerrainEdit> edit;
    QSharedPointer<Terrain> terrain;
};

static TerrainHandle createHandle(QSharedPointer<Terrain> t)
{
    return reinterpret_cast<TerrainHandle>(new QSharedPointer<Terrain>(t));
}
static TerrainEditHandle createHandle(LuaTerrainEdit e)
{
    return reinterpret_cast<TerrainEditHandle>(new LuaTerrainEdit(e));
}
static ToolEditorHandle createHandle(QSharedPointer<ToolEditor> t)
{
    return reinterpret_cast<ToolEditorHandle>(new QSharedPointer<ToolEditor>(t));
}
static QSharedPointer<Terrain> *fromTerrainHandle(TerrainHandle h)
{
    return reinterpret_cast<QSharedPointer<Terrain> *>(h);
}
static LuaTerrainEdit *fromTerrainEditHandle(TerrainEditHandle h)
{
    return reinterpret_cast<LuaTerrainEdit *>(h);
}
static QSharedPointer<ToolEditor> *fromToolEditorHandle(ToolEditorHandle h)
{
    return reinterpret_cast<QSharedPointer<ToolEditor> *>(h);
}
static LuaHost *toHost(Host host)
{
    return reinterpret_cast<LuaHost *>(host);
}

class LuaEffectController : public EffectController
{
    QString name_;
    TerravoxEffectController sctrl;
    QSharedPointer<ToolEditor> editor;
    TerravoxEffectApi api;
    LuaHost host;
public:
    LuaEffectController(QString name, TerravoxEffectControllerFactory factory, const LuaHost &host) :
        name_(name),
        host(host)
    {
        api.state = reinterpret_cast<void *>(this);
        api.preview = [](void *self_) {
            auto *self = reinterpret_cast<LuaEffectController *>(self_);
            self->preview();
        };

        factory(&api, &sctrl);

        if (sctrl.editor) {
            editor = *fromToolEditorHandle(sctrl.editor); // copy handle
        }
    }
    ~LuaEffectController()
    {
        host.pcall([=]{
            sctrl.destroy();
        });
    }

    QString name() override { return name_; }

protected:
    QWidget *createEffectEditor(Session *) Q_DECL_OVERRIDE
    {
        if (!editor) {
            return nullptr;
        }
        return editor->takeWidget();
    }

    void applyEffect(QSharedPointer<Terrain> t, QSharedPointer<TerrainEdit> edit, Session *) Q_DECL_OVERRIDE
    {
        LuaTerrainEdit e;
        e.edit = edit;
        e.terrain = t;
        edit->copyBeforeTo(QRect(QPoint(0, 0), t->size()), t.data(), t.data()); // restore to original
        host.pcall([=]{
            sctrl.apply(createHandle(e));
        });
        t->quantize(); // enforce quantization
    }

};

LuaScriptInterface::LuaScriptInterface(LuaInterface *interface, std::function<bool(std::function<void ()>)> pcall)
{
    auto &api = api_;
    auto &host = host_;

    host.interface = interface;
    host.pcall = pcall;
    api.host = reinterpret_cast<Host>(&host);

    api.aboutQt = []() {
        QApplication::aboutQt();
    };

    // Host api
    api.registerEffect = [](Host host, const char *name, TerravoxEffectControllerFactory factory) {
        auto *h = toHost(host);
        QString qname = name;
        h->interface->registerEffect(qname, [=]() -> EffectController * {
            EffectController *ret = nullptr;
            if (h->pcall([&](){ ret = new LuaEffectController(qname, factory, *h); })) {
                return ret;
            } else {
                return nullptr;
            }
        });
    };

    // Terrain API
    api.terrainCreate = [](int width, int height) -> TerrainHandle {
        auto t = QSharedPointer<Terrain>::create(QSize(width, height));
        return createHandle(t);
    };
    api.terrainRelease = [](TerrainHandle h) -> void {
        delete fromTerrainHandle(h);
    };
    api.terrainQuantize = [](TerrainHandle h) -> void {
        (*fromTerrainHandle(h))->quantize();
    };
    api.terrainGetColorData = [](TerrainHandle h) -> uint32_t* {
        return (*fromTerrainHandle(h))->colorData();
    };
    api.terrainGetLandformData = [](TerrainHandle h) -> float* {
        return (*fromTerrainHandle(h))->landformData();
    };
    api.terrainGetSize = [](TerrainHandle h, int *outDim) -> void {
        auto &t = (*fromTerrainHandle(h));
        outDim[0] = t->size().width();
        outDim[1] = t->size().height();
    };
    api.terrainClone = [](TerrainHandle h) -> TerrainHandle {
        auto &t = (*fromTerrainHandle(h));
        QSharedPointer<Terrain> newTer = QSharedPointer<Terrain>::create(t->size());
        newTer->copyFrom(t.data(), QPoint(0, 0));
        return createHandle(newTer);
    };

    // TerrainEdit API
    api.terrainEditRelease = [](TerrainEditHandle h) -> void {
        delete fromTerrainEditHandle(h);
    };
    api.terrainEditBeginEdit = [](TerrainEditHandle h, int x, int y, int width, int height) -> void {
        LuaTerrainEdit &e = *fromTerrainEditHandle(h);
        e.edit->beginEdit(QRect(x, y, width, height), e.terrain.data());
    };
    api.terrainEditEndEdit = [](TerrainEditHandle h) -> void {
        LuaTerrainEdit &e = *fromTerrainEditHandle(h);
        e.edit->endEdit(e.terrain.data());
    };
    api.terrainEditGetTerrain = [](TerrainEditHandle h) -> TerrainHandle {
        LuaTerrainEdit &e = *fromTerrainEditHandle(h);
        return createHandle(e.terrain);
    };

    // ToolEditor API
    api.toolEditorCreate = [](Host host) -> ToolEditorHandle {
        auto *h = toHost(host);
        return createHandle(QSharedPointer<ToolEditor>::create(*h));
    };
    api.toolEditorRelease = [](ToolEditorHandle h) -> void {
        delete fromToolEditorHandle(h);
    };
    api.toolEditorAddFinalizer = [](ToolEditorHandle h, void (*fin)()) -> void {
        (*fromToolEditorHandle(h))->finalizers.push_back(fin);
    };
    api.toolEditorAddIntegerSlider = [](ToolEditorHandle h, ToolEditorControlEventHandler callback, const char *text,
            int minValue, int maxValue, int logarithmic) -> int {
        QSharedPointer<ToolEditor> &e = *fromToolEditorHandle(h);
        auto cb = [=](ToolEditor::Control *ctrl) {
            if (callback)
                e->host.pcall([=](){
                    callback(ctrl->id);
                });
        };
        return e->addIntSlider(cb, text, minValue, maxValue, logarithmic);
    };
    api.toolEditorAddRealSlider = [](ToolEditorHandle h, ToolEditorControlEventHandler callback, const char *text,
            double minValue, double maxValue, int digits, int logarithmic) -> int {
        QSharedPointer<ToolEditor> &e = *fromToolEditorHandle(h);
        auto cb = [=](ToolEditor::Control *ctrl) {
            if (callback)
                e->host.pcall([=](){
                    callback(ctrl->id);
                });
        };
        return e->addRealSlider(cb, text, minValue, maxValue, digits, logarithmic);
    };
    api.toolEditorSetValue = [](ToolEditorHandle h, int controlId, double value) -> void {
        QSharedPointer<ToolEditor> &e = *fromToolEditorHandle(h);
        auto *ctrl = e->controlById(controlId);
        if (ctrl)
            ctrl->setValue(value);
    };
    api.toolEditorGetValue = [](ToolEditorHandle h, int controlId) -> double {
        QSharedPointer<ToolEditor> &e = *fromToolEditorHandle(h);
        auto *ctrl = e->controlById(controlId);
        if (ctrl)
            return ctrl->getValue();
        else
            return 0;
    };

}
