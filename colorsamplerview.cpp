#include "colorsamplerview.h"
#include <QPainter>
#include "terrainview.h"
#include <QTimer>
#include <QApplication>
#include <QPixmap>
#include "terrain.h"

ColorSamplerView::ColorSamplerView(TerrainView *view, QObject *parent) :
    QObject(parent),
    view(view),
    active(false),
    hover(false),
    otherActive(false),
    mouseHover(false)
{
    connect(view, SIGNAL(clientMousePressed(QMouseEvent*)),
            SLOT(clientMousePressed(QMouseEvent*)));
    connect(view, SIGNAL(clientMouseReleased(QMouseEvent*)),
            SLOT(clientMouseReleased(QMouseEvent*)));
    connect(view, SIGNAL(clientMouseMoved(QMouseEvent*)),
            SLOT(clientMouseMoved(QMouseEvent*)));
    connect(view, SIGNAL(clientPaint(QPaintEvent*)),
            SLOT(clientPaint(QPaintEvent*)));
    connect(view, SIGNAL(clientEnter(QEvent*)),
            SLOT(clientEnter(QEvent*)));
    connect(view, SIGNAL(clientLeave(QEvent*)),
            SLOT(clientLeave(QEvent*)));
    connect(view, SIGNAL(terrainPaint(TerrainViewDrawingContext*)),
            SLOT(terrainPaint(TerrainViewDrawingContext*)));

    timer.reset(new QTimer());
    timer->setInterval(30);
    connect(timer.data(), SIGNAL(timeout()), SLOT(checkModifier()));

    cursor = QCursor(QPixmap(":/Terravox/cursorEyedropper.png"), 5, 26);
}

ColorSamplerView::~ColorSamplerView()
{

}

void ColorSamplerView::clientMousePressed(QMouseEvent *e)
{
    if (e->isAccepted()) {
        otherActive = true;
    }
    checkModifier();

    if (active) {
        return;
    }
    if (hover && e->button() == Qt::LeftButton) {
        active = true;
        e->accept();
        clientMouseMoved(e);
    } else {
        otherActive = true;
    }
}

void ColorSamplerView::clientMouseReleased(QMouseEvent *e)
{
    if (e->buttons() == 0) {
        otherActive = false;
    }
    if (e->button() == Qt::LeftButton) {
        active = false;
    }
    checkModifier();
}

void ColorSamplerView::clientMouseMoved(QMouseEvent *e)
{
    cursorPos = e->pos();
    checkModifier();
    if (hover) {
        // sample
        lastSampledColor = QColor();
        if (QRect(QPoint(0, 0), view->size()).contains(e->pos())) {
            auto p = view->rayCast(e->pos());
            Terrain *t = view->terrain().data();
            auto tsz = t->size();
            if (p.x() >= 0 && p.y() >= 0 && p.x() < tsz.width() && p.y() < tsz.height()) {
                lastSampledColor = QColor::fromRgb(t->color(p.x(), p.y()) | 0xff000000);

            }
        }

        if (active && lastSampledColor.isValid()) {
            emit sampled(lastSampledColor);
        }
        view->update();
    }
}

void ColorSamplerView::clientPaint(QPaintEvent *)
{
    if (!hover)
        return;
    QPainter p(view);
    p.setRenderHint(QPainter::Antialiasing);
    if (lastSampledColor.isValid()) {
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(qRgba(0, 0, 0, 128)));
        p.drawEllipse(cursorPos, 7, 7);
        p.setBrush(QColor(qRgba(255, 255, 255, 255)));
        p.drawEllipse(cursorPos, 6, 6);
        p.setBrush(lastSampledColor);
        p.drawEllipse(cursorPos, 5, 5);
    }
}

void ColorSamplerView::clientEnter(QEvent *)
{
    //timer->start(); // FIXME: find better way...
    mouseHover = true;
    checkModifier();
}

void ColorSamplerView::clientLeave(QEvent *)
{
    //timer->stop();
    mouseHover = false;
    checkModifier();
}

void ColorSamplerView::terrainPaint(TerrainViewDrawingContext *dc)
{

}

void ColorSamplerView::checkModifier(Qt::KeyboardModifiers km)
{
    bool state = km & Qt::AltModifier;
    state = state && mouseHover;
    if (active) {
        state = true;
    }
    if (otherActive) {
        state = false;
    }
    if (state != hover) {
        if (state) {
            view->addCursorOverride(&cursor);
        } else {
            view->removeCursorOverride(&cursor);
        }
        view->update();
        hover = state;
    }
}

void ColorSamplerView::checkModifier()
{
    checkModifier(QApplication::keyboardModifiers());
}

