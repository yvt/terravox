#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QWidget>
#include <QSharedPointer>
#include <functional>
#include <QScopedPointer>
#include <QHash>
#include <QToolButton>

class Session;
class ToolView;

namespace Ui {
class MainWindow;
}

class ToolController;
class EffectController;
class ColorPickerWindow;
class ColorSamplerView;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();

private:
    Ui::MainWindow *ui;

    QSharedPointer<Session> session;
    QString currentFilePath;

    QScopedPointer<ColorSamplerView> colorSampler;

    QWidget *currentToolEditor;
    QSharedPointer<ToolView> currentToolView;

    QScopedPointer<ColorPickerWindow> primaryColorPickerWindow;

    bool modified;
    bool closeForced;

    QSharedPointer<ToolController> currentTool; // FIXME: is this needed?

    QSharedPointer<ToolController> lastTool; // used after effect is done

    bool isEffectInstanceActive() const { return lastTool; }
    void startEffectInstance(QSharedPointer<EffectController> fx);
    void startEffectInstanceInterractive(QSharedPointer<EffectController> fx);
    /** true = okay, false = callback() will be called later... */
    bool makeSureEffectIsDone(std::function<void(bool)> callback);

    QHash<ToolController *, QToolButton *> toolButtons;
    void updateUIToolSelection();

    bool confirmClose(std::function<void(bool)> callback);
    bool saveInteractive(std::function<void(bool)> callback);
    void saveAsInteractive(std::function<void(bool)> callback);
    bool saveTo(const QString &path, QString &error);

    void setSession(QSharedPointer<Session> session);

    void updateUI();

protected:

    void closeEvent(QCloseEvent *) override;

private slots:
    void terrainUpdated();
    void toolChanged();
    void on_actionShow_View_Options_triggered();
    void on_actionAbout_Qt_triggered();
    void on_actionAbout_triggered();
    void on_actionOpen_triggered();
    void on_actionSave_triggered();
    void on_actionSaveAs_triggered();
    void on_actionNewTerrainPlain_triggered();
    void on_actionNewTerrainFractal_triggered();
    void primaryColorClicked();
    void primaryColorPickerClosed();
    void primaryColorEdited();
    void effectDone();
    void on_action_Erosion_triggered();
};

#endif // MAINWINDOW_H
