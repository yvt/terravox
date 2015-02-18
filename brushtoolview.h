#ifndef BRUSHTOOLVIEW_H
#define BRUSHTOOLVIEW_H

#include "toolcontroller.h"
#include <QPoint>
#include <QImage>
#include <QScopedPointer>
#include <QElapsedTimer>
#include <QTimer>

class BrushTool;
class TerrainViewDrawingContext;
class QMouseEvent;
class BrushToolEdit;
class Session;

class BrushToolView : public ToolView
{
    Q_OBJECT

    QImage tipImage;
public:
    BrushToolView(TerrainView *, Session *, BrushTool *);
    ~BrushToolView();

private:
    TerrainView *view;
    BrushTool *tool;
    QPoint cursor;
    QPoint lastCursor;
    Session *session;

    QPoint dragStartPos;
    QElapsedTimer elapsedTimer;
    QScopedPointer<QTimer> timer;
    float adjustedStrength;

    QScopedPointer<BrushToolEdit> currentEdit;

    void paint(bool interpolated = false);

private slots:
    void clientMousePressed(QMouseEvent *);
    void clientMouseReleased(QMouseEvent *);
    void clientMouseMoved(QMouseEvent *);
    void terrainPaint(TerrainViewDrawingContext *);
    void parameterChanged();
    void timerTicked();
};

#endif // BRUSHTOOLVIEW_H
