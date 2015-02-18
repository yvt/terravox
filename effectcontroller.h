#ifndef EFFECTCONTROLLER_H
#define EFFECTCONTROLLER_H

#include "toolcontroller.h"

#include <QScopedPointer>
#include <QSharedPointer>
#include <QEnableSharedFromThis>

/*
 * Here's how effect works:
 *
 * 1. application creates EffectController.
 * 2. EffectController.createEditor and createView is called.
 *    Effect previews the result by modifying Session.terrain.
 * 3. One of either actions is performed:
 *     a. User clicks "Apply" or "Revert" in EffectEditor.
 *        EffectController.apply() or EffectController.revert() will be called.
 *     b. Application calls EffectController.leave().
 *        EffectController.apply() or EffectController.revert() will be called by leave().
 * 4. After effect is done, the signal EffectController.done() is emitted.
 *
 */
class Terrain;
class TerrainEdit;

class EffectController : public ToolController, public QEnableSharedFromThis<EffectController>
{
    Q_OBJECT
public:
    explicit EffectController(QObject *parent = 0);
    ~EffectController();

    QWidget *createEditor(Session *) override final;
    bool leave(std::function<void(bool)> callback) override;

signals:
    void applied();
    void reverted();
    void done();

protected:
    virtual QWidget *createEffectEditor(Session *) = 0;
    virtual void applyEffect(Terrain *, QSharedPointer<TerrainEdit>, Session *) = 0;

protected slots:
    void preview();

private slots:
    void apply();
    void revert();

private:
    QWidget *editor;
    Session *session;
    QSharedPointer<TerrainEdit> previewEdit;

private slots:
    void editorDestroyed(QObject *);
};

#endif // EFFECTCONTROLLER_H
