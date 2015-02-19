#ifndef COLORSAMPLERVIEW_H
#define COLORSAMPLERVIEW_H

#include <QObject>
#include <QScopedPointer>
#include <QInputEvent>
#include <QCursor>
#include <QColor>
#include <QPoint>

class TerrainViewDrawingContext;
class TerrainView;

class ColorSamplerView : public QObject
{
    Q_OBJECT
public:
    explicit ColorSamplerView(TerrainView *view, QObject *parent = 0);
    ~ColorSamplerView();

signals:
    void sampled(QColor);

private slots:
    void clientMousePressed(QMouseEvent *);
    void clientMouseReleased(QMouseEvent *);
    void clientMouseMoved(QMouseEvent *);
    void clientPaint(QPaintEvent *);
    void clientEnter(QEvent *);
    void clientLeave(QEvent *);
    void terrainPaint(TerrainViewDrawingContext *);

    void checkModifier(Qt::KeyboardModifiers);
    void checkModifier();

private:
    TerrainView *view;
    QScopedPointer<QTimer> timer;
    bool mouseHover;
    bool hover;
    bool active;
    bool otherActive; // other tool is using mouse
    QPoint cursorPos;
    QCursor cursor;
    QColor lastSampledColor;
};

#endif // COLORSAMPLERVIEW_H
