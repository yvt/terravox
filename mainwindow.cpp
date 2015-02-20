
#include <QMessageBox>
#include <QFileDialog>
#include <QCloseEvent>
#include <QFile>
#include <QDir>
#include <QLabel>
#include <QSharedPointer>

#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QHBoxLayout>
#include "session.h"
#include "colorpickerwindow.h"
#include "colorpicker.h"
#include "colorsamplerview.h"

#include "toolcontroller.h"
#include "brushtoolcontroller.h"
#include "emptytooleditor.h"
#include "effectcontroller.h"
#include "erosioneffectcontroller.h"
#include "manipulatetoolcontroller.h"

#include "terraingenerator.h"
#include "terrain.h"

#include "vxl.h"


MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow),
    currentToolEditor(nullptr),
    modified(false),
    settings(new QSettings()),
    closeForced(false)
{
    ui->setupUi(this);

    // Application settings
    ui->primaryColorView->setValue(settings->value("primaryColor", QColor(255, 255, 255)).value<QColor>());
    connect(ui->primaryColorView, &ColorView::valueChanged, [&](QColor color) {
        settings->setValue("primaryColor", color);
    });

    {
        TerrainViewOptions options = ui->terrainView->viewOptions();
        settings->beginGroup("terrainview");
        options.ambientOcclusion = settings->value("ambientOcclusion", options.ambientOcclusion).value<bool>();
        options.ambientOcclusionStrength =
                settings->value("ambientOcclusionStrength", options.ambientOcclusionStrength).value<float>();
        options.axises = settings->value("axises", options.axises).value<bool>();
        options.colorizeAltitude = settings->value("colorizeAltitude", options.colorizeAltitude).value<bool>();
        options.showEdges = settings->value("showEdges", options.showEdges).value<bool>();
        settings->endGroup();
        ui->terrainView->setViewOptions(options);

        connect(ui->terrainView, &TerrainView::viewOptionsChanged, [&](TerrainViewOptions options) {
            settings->beginGroup("terrainview");
            settings->setValue("ambientOcclusion", options.ambientOcclusion);
            settings->setValue("ambientOcclusionStrength", options.ambientOcclusionStrength);
            settings->setValue("axises", options.axises);
            settings->setValue("colorizeAltitude", options.colorizeAltitude);
            settings->setValue("showEdges", options.showEdges);
            settings->endGroup();
        });
    }


    // Connect color picker's signal
    connect(ui->primaryColorView, SIGNAL(clicked()),
            SLOT(primaryColorClicked()));
    connect(ui->primaryColorView, SIGNAL(valueChanged(QColor)),
            SLOT(primaryColorEdited()));
    // FIXME: session.primaryColor -> primaryColorView.value

    // Add tools
    auto addTool = [&](ToolController *tool, QToolButton *button) {
        auto ptr = QSharedPointer<ToolController>(tool);
        auto setActive = [this, ptr]() {
            if (session->tool() == ptr) {
                return;
            }
            auto action = [=](bool b) {
                if (b)
                    session->setTool(ptr);
                else
                    // changin tool was denied.
                    updateUIToolSelection();
            };
            if (!session->tool() || session->tool()->leave(action)) {
                action(true);
            }
        };

        connect(button, &QToolButton::toggled, [setActive](bool value) {
           if (value) {
               setActive();
           }
        });

        toolButtons[ptr.data()] = button;
    };
    addTool(nullptr, ui->toolPan);
    addTool(new BrushToolController(BrushType::Raise), ui->toolRaise);
    addTool(new BrushToolController(BrushType::Lower), ui->toolLower);
    addTool(new BrushToolController(BrushType::Smoothen), ui->toolSmoothen);
    addTool(new BrushToolController(BrushType::Paint), ui->toolPaint);
    addTool(new BrushToolController(BrushType::Blur), ui->toolBlur);
    addTool(new ManipulateToolController(ManipulateMode::Landform), ui->toolManipulateHeight);
    addTool(new ManipulateToolController(ManipulateMode::Color), ui->toolManipulateColor);

    colorSampler.reset(new ColorSamplerView(ui->terrainView));
    connect(colorSampler.data(), SIGNAL(sampled(QColor)),
            ui->primaryColorView, SLOT(setValue(QColor)));

    QSharedPointer<Session> s = QSharedPointer<Session>::create();
    s->setTerrain(QSharedPointer<Terrain>(TerrainGenerator(QSize(512, 512)).generateRandomLandform()));
    s->terrain()->quantize();

    setSession(s);

    ui->toolRaise->setChecked(true);

    updateUI();
}

void MainWindow::setSession(QSharedPointer<Session> newSession)
{
    if (session) {
        session->disconnect(this);
    }

    session = newSession;
    connect(session.data(), SIGNAL(terrainUpdated(QRect)),
            SLOT(terrainUpdated()));
    connect(session.data(), SIGNAL(terrainUpdated(QRect)),
            ui->terrainView, SLOT(terrainUpdate(QRect)));
    connect(session.data(), SIGNAL(toolChanged()),
            SLOT(toolChanged()));

    connect(session.data(), SIGNAL(canUndoChanged(bool)),
            ui->action_Undo, SLOT(setEnabled(bool)));
    connect(ui->action_Undo, SIGNAL(triggered()),
            session.data(), SLOT(undo()));
    ui->action_Undo->setEnabled(session->canUndo());

    connect(session.data(), SIGNAL(canRedoChanged(bool)),
            ui->action_Redo, SLOT(setEnabled(bool)));
    ui->action_Redo->setEnabled(session->canRedo());
    connect(ui->action_Redo, SIGNAL(triggered()),
            session.data(), SLOT(redo()));


    ui->terrainView->setTerrain(session->terrain());
    session->setTool(currentTool);

    primaryColorEdited(); // set primary color of session
}

void MainWindow::updateUI()
{
    QString appName = QApplication::applicationName();

    setWindowTitle(tr("%2[*] - %1").arg(appName, currentFilePath.isEmpty() ? tr("Untitled") : QFileInfo(currentFilePath).fileName()));

    setWindowModified(modified);
}

MainWindow::~MainWindow()
{
    currentToolView.reset();
    if (currentToolEditor) {
        delete currentToolEditor;
        currentToolEditor = nullptr;
    }

    currentToolView.reset();

    if (primaryColorPickerWindow)
        primaryColorPickerWindow->close();

    delete ui;
}

void MainWindow::startEffectInstance(QSharedPointer<EffectController> fx)
{
    Q_ASSERT(!isEffectInstanceActive());
    Q_ASSERT(fx);
    Q_ASSERT(session->tool());

    lastTool = session->tool();
    session->setTool(fx);

    connect(fx.data(), SIGNAL(done()), SLOT(effectDone()));

    updateUIToolSelection();
}

void MainWindow::startEffectInstanceInterractive(QSharedPointer<EffectController> fx)
{
    auto cb = [=](bool result) {
        if (result)
            startEffectInstance(fx);
    };

    if (makeSureEffectIsDone(cb)) {
        cb(true);
    }
}

void MainWindow::effectDone()
{
    Q_ASSERT(isEffectInstanceActive());

    disconnect(this, SLOT(effectDone()));

    session->setTool(lastTool);
    lastTool.reset();

}

bool MainWindow::makeSureEffectIsDone(std::function<void (bool)> callback)
{
    if (!isEffectInstanceActive())
        return true;
    Q_ASSERT(session->tool());
    return session->tool()->leave(callback);
}

void MainWindow::updateUIToolSelection()
{
    auto *current = session->tool().data();
    bool none = true;
    foreach (ToolController *tool, toolButtons.keys())
    {
        toolButtons[tool]->setAutoExclusive(false);
        toolButtons[tool]->setChecked(current == tool);
        if (current == tool) {
            none = false;
        }
        toolButtons[tool]->setAutoExclusive(true);
    }
    if (none) {
        // uncheck all
        // http://qt-project.org/forums/viewthread/5482
        foreach (ToolController *tool, toolButtons.keys())
        {
            toolButtons[tool]->setAutoExclusive(false);
            toolButtons[tool]->setCheckable(false);
        }
        foreach (ToolController *tool, toolButtons.keys())
        {
            toolButtons[tool]->setCheckable(true);
            toolButtons[tool]->setAutoExclusive(true);
        }
    }
}

bool MainWindow::saveInteractive(std::function<void (bool)> callback)
{
    if (currentFilePath.isEmpty()) {
        saveAsInteractive(callback);
        return false;
    }

    QString error;
    if (saveTo(currentFilePath, error)) {
        modified = false;
        updateUI();
        return true;
    }

    QMessageBox *msgbox = new QMessageBox(
                QMessageBox::Warning, QApplication::applicationName(), tr("Could not save file."),
                QMessageBox::Ok, this);
    msgbox->setInformativeText(error);
    msgbox->setWindowFlags(Qt::Sheet);
    msgbox->show();

    connect(msgbox, &QMessageBox::finished, [msgbox](int) {
        msgbox->deleteLater();
    });
    return false;

}

void MainWindow::saveAsInteractive(std::function<void (bool)> callback)
{
    QString fn = QFileDialog::getSaveFileName(this, tr("Save As"), currentFilePath,
                                              tr("Voxlap5 512x512x64 VXL (*.vxl)"));

    if (fn.isEmpty()) {
        callback(false);
        return;
    }

    auto showError = [&](QString msg) {
        QMessageBox *msgbox = new QMessageBox(
                    QMessageBox::Warning, QApplication::applicationName(), tr("Could not save file."),
                    QMessageBox::Ok, this);
        msgbox->setWindowFlags(Qt::Sheet);
        msgbox->setInformativeText(msg);
        connect(msgbox, &QMessageBox::finished, [=]() mutable {msgbox->deleteLater();});
        msgbox->show();
        callback(false);
    };

    QString error;
    if (!saveTo(fn, error)) {
        showError(error);
        return;
    }

    currentFilePath = fn;
    modified = false;
    updateUI();

    callback(true);
}

bool MainWindow::saveTo(const QString &path, QString &error)
{
    QByteArray bytes;
    if (!vxl::save(bytes, session->terrain().data())) {
        error = tr("Failed to encode VXL data.");
        return false;
    }

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        error = file.errorString();
        return false;
    }

    if (file.write(bytes) < bytes.size()) {
        error = file.errorString();
        return false;
    }

    file.close();
    return true;
}

bool MainWindow::confirmClose(std::function<void(bool)> callback)
{
    auto action = [=]() -> bool {
        if (!modified) {
            return true;
        }

        QMessageBox *msgbox = new QMessageBox(
                    QMessageBox::Warning, QApplication::applicationName(), tr("Terrain has been changed."),
                    QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel, this);
        msgbox->setInformativeText(tr("Do you want to save the terrain?"));
        msgbox->setWindowFlags(Qt::Sheet);
        msgbox->setDefaultButton(QMessageBox::Save);
        msgbox->show();

        connect(msgbox, &QMessageBox::finished, [this, msgbox, callback](int button) {
            switch (button) {
            case QMessageBox::Save:
                if (saveInteractive(callback)) {
                    callback(true);
                }
                break;
            case QMessageBox::Discard:
                callback(true);
                break;
            case QMessageBox::Cancel:
                callback(false);
                break;
            }
            msgbox->deleteLater();
        });
        return false;
    };
    auto later = [=](bool result) {
        if (result) {
            if (action()) {
                callback(true);
            }
        } else {
            callback(false);
        }
    };
    if (makeSureEffectIsDone(later)) {
        return action();
    } else {
        return false;
    }
}

void MainWindow::closeEvent(QCloseEvent *e)
{
    if (closeForced) {
        return;
    }
    if (!confirmClose([=](bool success) {
       if (success) { closeForced = true; close(); }
    })) {
        e->ignore();
    }
}

void MainWindow::terrainUpdated()
{
    ui->terrainView->update();
    modified = true;
    updateUI();
}

void MainWindow::toolChanged()
{
    if (currentToolEditor) {
        delete currentToolEditor;
        currentToolEditor = nullptr;
    }

    currentTool = session->tool();

    auto *layout = qobject_cast<QHBoxLayout *>(ui->toolEditorFrame->layout());
    if (session->tool())
        currentToolEditor = session->tool()->createEditor(session.data());
    else
        currentToolEditor = new EmptyToolEditor();
    if (currentToolEditor == nullptr)
        currentToolEditor = new EmptyToolEditor();

    // tool name label
    QString toolName = tr("Pan");
    if (session->tool()) {
        toolName = session->tool()->name();
    }
    QLabel *label = ui->toolNameLabel;
    label->setText(toolName);

    if (currentToolEditor)
        layout->addWidget(currentToolEditor, 1);


    if (session->tool())
        currentToolView = QSharedPointer<ToolView>(session->tool()->createView(session.data(), ui->terrainView));
    else
        currentToolView.reset();

    updateUIToolSelection();
}

void MainWindow::on_actionShow_View_Options_triggered()
{
    ui->terrainView->showOptionsWindow();
}

void MainWindow::on_actionAbout_Qt_triggered()
{
    QMessageBox::aboutQt(this);
}

void MainWindow::on_actionAbout_triggered()
{
    QMessageBox::about(this, tr("About Terravox"), tr("Terravox (version %1)").arg(tr("unknown")));
}

void MainWindow::on_actionOpen_triggered()
{
    auto doAction = [=]() {
        QString fn = QFileDialog::getOpenFileName(this, tr("Open"),
            QString(), tr("Voxlap5 512x512x64 VXL (*.vxl)"));
        if (fn.isEmpty())
        {
            return;
        }

        auto showError = [&](QString msg) {
            QMessageBox *msgbox = new QMessageBox(
                        QMessageBox::Warning, QApplication::applicationName(), tr("Could not open file."),
                        QMessageBox::Ok, this);
            msgbox->setWindowFlags(Qt::Sheet);
            msgbox->setInformativeText(msg);
            connect(msgbox, &QMessageBox::finished, [=]() mutable {msgbox->deleteLater();});
            msgbox->show();
        };

        QFile file(fn);
        if (!file.open(QIODevice::ReadOnly)) {
            showError(file.errorString());
            return;
        }

        auto bytes = file.readAll();
        if (file.error()) {
            showError(file.errorString());
            return;
        }

        QString msgOut;
        auto *t = vxl::load(bytes, msgOut);
        if (!t) {
            showError(msgOut);
            return;
        }

        QSharedPointer<Session> s = QSharedPointer<Session>::create();
        s->setTerrain(QSharedPointer<Terrain>(t));

        setSession(s);

        if (!msgOut.isEmpty()) {
            currentFilePath = ""; // overwriting would result in loss
            updateUI();

            QMessageBox *msgbox = new QMessageBox(
                        QMessageBox::Warning, QApplication::applicationName(), tr("File could be opened successfully, but there was a problem."),
                        QMessageBox::Ok,
                        this);
            msgbox->setWindowFlags(Qt::Sheet);
            msgbox->setInformativeText(msgOut);
            connect(msgbox, &QMessageBox::finished, [=]() mutable {msgbox->deleteLater();});
            msgbox->show();
        } else {
            currentFilePath = fn;
            updateUI();
        }

    };

    if (confirmClose([=](bool result) {
        if (result) {
            doAction();
        }
    })) {
        doAction();
    }
}

void MainWindow::on_actionSave_triggered()
{
    saveInteractive([](bool){});
}

void MainWindow::on_actionSaveAs_triggered()
{
    saveAsInteractive([](bool){});
}

void MainWindow::on_actionNewTerrainPlain_triggered()
{
    auto doAction = [&]{

        currentFilePath = "";
        modified = false;

        QSharedPointer<Session> s = QSharedPointer<Session>::create();
        s->setTerrain(QSharedPointer<Terrain>::create(QSize(512, 512)));

        auto *t = s->terrain().data();
        for (int y = 0; y < 512; ++y) {
            for (int x = 0; x < 512; ++x) {
                t->landform(x, y) = 63.f;
                t->color(x, y) = 0xafafaf;
            }
        }

        setSession(s);

        updateUI();
    };
    if (confirmClose([=](bool result) {
        if (result) {
            doAction();
        }
    })) {
        doAction();
    }
}

void MainWindow::on_actionNewTerrainFractal_triggered()
{
    auto doAction = [&]{

        currentFilePath = "";
        modified = false;

        QSharedPointer<Session> s = QSharedPointer<Session>::create();
        s->setTerrain(QSharedPointer<Terrain>(TerrainGenerator(QSize(512, 512)).generateRandomLandform()));
        s->terrain()->quantize();

        setSession(s);

        updateUI();
    };
    if (confirmClose([=](bool result) {
        if (result) {
            doAction();
        }
    })) {
        doAction();
    }
}

void MainWindow::primaryColorClicked()
{
    if (!primaryColorPickerWindow) {
        primaryColorPickerWindow.reset(new ColorPickerWindow(this));
        primaryColorPickerWindow->setWindowTitle(tr("Primary Color"));
        connect(primaryColorPickerWindow.data(), SIGNAL(destroyed()),
                SLOT(primaryColorPickerClosed()));

        // make sure picker and view is linked
        primaryColorPickerWindow->colorPicker()->setValue(ui->primaryColorView->value());
        connect(ui->primaryColorView, SIGNAL(valueChanged(QColor)),
                primaryColorPickerWindow->colorPicker(), SLOT(setValue(QColor)));
        connect(primaryColorPickerWindow->colorPicker(), SIGNAL(valueChanged(QColor)),
                ui->primaryColorView, SLOT(setValue(QColor)));

    }
    primaryColorPickerWindow->show();
}

void MainWindow::primaryColorPickerClosed()
{
    if (primaryColorPickerWindow) {
        primaryColorPickerWindow.take()->deleteLater();
    }
}

void MainWindow::primaryColorEdited()
{
    session->setPrimaryColor(ui->primaryColorView->value().toRgb().rgb());
}

void MainWindow::on_action_Erosion_triggered()
{
    startEffectInstanceInterractive(QSharedPointer<ErosionEffectController>::create());
}
