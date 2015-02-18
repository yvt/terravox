#ifndef EROSIONEFFECTCONTROLLER_H
#define EROSIONEFFECTCONTROLLER_H

#include "effectcontroller.h"
#include <QScopedPointer>

class ErosionEffect;

class ErosionEffectController : public EffectController
{
    QScopedPointer<ErosionEffect> fx;
public:
    ErosionEffectController(QObject *parent = 0);
    ~ErosionEffectController();

    QString name() override;

protected:
    QWidget *createEffectEditor(Session *) override;
    void applyEffect(Terrain *, QSharedPointer<TerrainEdit>, Session *) override;
};

#endif // EROSIONEFFECTCONTROLLER_H
