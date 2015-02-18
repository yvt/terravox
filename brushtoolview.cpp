#include "brushtoolview.h"
#include "brushtool.h"
#include <QMouseEvent>
#include "terrainview.h"
#include "terrain.h"
#include "session.h"
#include "terrainedit.h"

static const QPoint InvalidPoint(-1000, -1000);

BrushToolView::BrushToolView(TerrainView *view, Session *session, BrushTool *tool) :
    view(view),
    tool(tool),
    cursor(InvalidPoint),
    lastCursor(InvalidPoint),
    session(session)
{
    connect(view, SIGNAL(clientMousePressed(QMouseEvent*)),
            SLOT(clientMousePressed(QMouseEvent*)));
    connect(view, SIGNAL(clientMouseReleased(QMouseEvent*)),
            SLOT(clientMouseReleased(QMouseEvent*)));
    connect(view, SIGNAL(clientMouseMoved(QMouseEvent*)),
            SLOT(clientMouseMoved(QMouseEvent*)));
    connect(view, SIGNAL(terrainPaint(TerrainViewDrawingContext*)),
            SLOT(terrainPaint(TerrainViewDrawingContext*)));

    connect(tool, SIGNAL(parameterChanged()), SLOT(parameterChanged()));

    timer.reset(new QTimer());
    connect(timer.data(), SIGNAL(timeout()), SLOT(timerTicked()));
    timer->setInterval(30);

    adjustedStrength = 0.5f;
}

BrushToolView::~BrushToolView()
{
    disconnect();

    timer.reset();
}

void BrushToolView::clientMousePressed(QMouseEvent *e)
{
    e->accept();

    if (cursor == InvalidPoint) {
        return;
    }

    lastCursor = cursor;

    auto tedit = session->beginEdit();
    if (!tedit) {
        // couldn't start editing.
        return;
    }

    dragStartPos = e->pos();

    // make sure tool color is set to session's primary color
    {
        auto p = tool->parameters();
        if (p.color != session->primaryColor()) {
            p.color = session->primaryColor();
            tool->setParameters(p);
        }
    }

    currentEdit.reset(tool->edit(tedit.data(), session->terrain().data()));
    if (currentEdit) {
        elapsedTimer.start();
        if (currentEdit->parameters().pressureMode == BrushPressureMode::AirBrush) {
            timer->start();
        } else if (currentEdit->parameters().pressureMode == BrushPressureMode::Constant) {
            currentEdit->draw(cursor, 1.f);
        } else {
            currentEdit->draw(cursor, adjustedStrength);
        }
        view->update();
    }
}

void BrushToolView::clientMouseReleased(QMouseEvent *e)
{
    if (currentEdit) {
        if (currentEdit->parameters().pressureMode != BrushPressureMode::Adjustable) {
            currentEdit->currentEdit()->beginEdit(currentEdit->currentEdit()->modifiedBounds(), session->terrain().data());
            session->terrain()->quantize();
            currentEdit->currentEdit()->endEdit(session->terrain().data());
        } else {
            adjustedStrength = std::max(std::min(adjustedStrength, 1.f), 0.f);
        }
        currentEdit.reset();
        timer->stop();;
        session->endEdit();
    }
}

void BrushToolView::clientMouseMoved(QMouseEvent *e)
{
    lastCursor = cursor;
    if (!currentEdit || currentEdit->parameters().pressureMode != BrushPressureMode::Adjustable) {
        cursor = view->rayCast(e->pos());

        auto *t = view->terrain().data();
        if (cursor.x() < 0 || cursor.y() < 0 ||
            cursor.x() >= t->size().width() ||
            cursor.y() >= t->size().height()) {
            cursor = InvalidPoint;
        } else {
            auto s = tool->parameters().size;
            cursor -= QPoint(s >> 1, s >> 1);
        }
        view->update();
    }

    if (currentEdit) {
        if (currentEdit->parameters().pressureMode == BrushPressureMode::Adjustable) {
            adjustedStrength -= (e->pos().y() - dragStartPos.y()) * (tool->type() == BrushType::Lower ? -0.01f : 0.01f);
            dragStartPos = e->pos();
            paint();
        } else {
            paint(true);
            if (timer->isActive()) {
                timer->stop(); timer->start();
            }
        }
    }

}

void BrushToolView::terrainPaint(TerrainViewDrawingContext *ctx)
{
    if (cursor == InvalidPoint) {
        return;
    }
    if (currentEdit) {
        return;
    }
    if (tipImage.isNull()) {
        Terrain *t = tool->tip().data();
        int width = t->size().width();
        int height = t->size().height();
        tipImage = QImage(width, height, QImage::Format_RGB32);

        auto *pixels = reinterpret_cast<quint32 *>(tipImage.bits());
        for (int y = 0; y < height; ++y)
            for (int x = 0; x < width; ++x)
                pixels[y * width + x] = t->landform(x, y) != 0.f ? 0x30ff00 : 0;

    }

    ctx->projectBitmap(tipImage, QPointF(cursor.x(), cursor.y()), QSizeF(tipImage.size().width(), tipImage.size().height()), true);
}

void BrushToolView::parameterChanged()
{
    tipImage = QImage();
}

void BrushToolView::timerTicked()
{
    paint();
}

void BrushToolView::paint(bool interpolated)
{
    if (!currentEdit) {
        return;
    }
    if (currentEdit->parameters().pressureMode == BrushPressureMode::Adjustable) {
        currentEdit->draw(cursor, std::max(std::min(adjustedStrength, 1.f), 0.f));
    } else {
        float amt = elapsedTimer.elapsed() / 1000.f;
        elapsedTimer.start();
        if (interpolated && lastCursor != InvalidPoint && cursor != InvalidPoint && lastCursor != cursor) {
            QPoint diff = cursor - lastCursor;
            float dist = std::sqrtf(diff.x() * diff.x() + diff.y() * diff.y());
            float interval = currentEdit->parameters().size / 8 + 1;
            int steps = std::max(1, static_cast<int>(std::ceil(dist / interval)));
            float x1 = lastCursor.x(), y1 = lastCursor.y();
            float dx = (cursor.x() - x1) / steps, dy = (cursor.y() - y1) / steps;

            for (int i = 0; i <= steps; ++i) {
                currentEdit->draw(QPoint(std::round(x1), std::round(y1)), amt / (steps + 1));
                x1 += dx; y1 += dy;
            }
            lastCursor = cursor;
        } else {
            if (cursor == InvalidPoint) {
                return;
            }
            currentEdit->draw(cursor, amt);
        }
    }
}

