#include "terrainview.h"
#include <QPaintEvent>
#include <QPainter>
#include "terrain.h"
#include <emmintrin.h>
#include "terraingenerator.h"
#include <QVector3D>
#include <QVector2D>
#include <cmath>
#include <QtConcurrent>
#include <QDebug>
#include "terrainviewoptionswindow.h"
#include "terrainview_p.h"

TerrainView::TerrainView(QWidget *parent) :
    QWidget(parent),
    d_ptr(new TerrainViewPrivate(this))
{
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
}

TerrainView::~TerrainView()
{
    delete d_ptr;
}

QPoint TerrainView::rayCast(const QPoint &p)
{
    Q_D(TerrainView);
    if (p.x() <= 0 || p.y() <= 0 || p.x() >= d->image_->size().width() ||
            p.y() >= d->image_->size().height()) {
        return QPoint(-1000, -1000);
    }
    auto pt = d->rayCastByDepthBuffer(p);
    return QPoint(std::floor(pt.x()), std::floor(pt.y()));
}

void TerrainView::setTerrain(QSharedPointer<Terrain> terrain)
{
    Q_D(TerrainView);
    if (d->terrain_ == terrain) {
        return;
    }
    d->terrain_ = terrain;
    if (terrain)
        d->terrainUpdate(QRect(QPoint(0, 0), terrain->size()));
    update();
}

QSharedPointer<Terrain> TerrainView::terrain()
{
    Q_D(TerrainView);
    return d->terrain_;
}

void TerrainView::setViewOptions(const TerrainViewOptions &opt)
{
    Q_D(TerrainView);
    d->viewOptions = opt;
    update();
}

const TerrainViewOptions &TerrainView::viewOptions()
{
    Q_D(TerrainView);
    return d->viewOptions;
}

void TerrainView::paintEvent(QPaintEvent *e)
{
    Q_D(TerrainView);
    auto size = this->size();


    SceneDefinition def;
    def.cameraDir = QVector3D(std::cos(d->yaw_) * std::cos(d->pitch_),
                              std::sin(d->yaw_) * std::cos(d->pitch_),
                              std::sin(d->pitch_));
    def.eye = d->centerPos_ - def.cameraDir;
    def.viewWidth = size.width() / d->scale_;
    def.viewHeight = size.height() / d->scale_;

    auto img = d->render(size, def);

    QPainter painter(this);
    painter.drawImage(0, 0, img);

    emit clientPaint(e);
}

void TerrainView::mousePressEvent(QMouseEvent *e)
{
    Q_D(TerrainView);

    if (e->button() != Qt::RightButton) {
        e->setAccepted(false);
        emit clientMousePressed(e);
        if (e->isAccepted()) {
            return;
        }
    }

    d->dragging_ = true;
    d->dragPos_ = e->pos();
    d->dragStartYaw_ = d->yaw_;
    d->dragStartPitch_ = d->pitch_;
    d->dragStartCenterPos_ = d->centerPos_;
    d->dragShiftMode_ = ((e->modifiers() & Qt::ShiftModifier) != 0 == ((e->buttons() & Qt::RightButton) != 0));


    e->accept();
}

void TerrainView::mouseReleaseEvent(QMouseEvent *e)
{
    Q_D(TerrainView);

    if (d->dragging_) {
        d->dragging_ = false;
        e->accept();
    } else {
        emit clientMouseReleased(e);
    }
}

void TerrainView::mouseMoveEvent(QMouseEvent *e)
{
    Q_D(TerrainView);
    if (d->dragging_) {
        bool shiftMode = ((e->modifiers() & Qt::ShiftModifier) != 0 == ((e->buttons() & Qt::RightButton) != 0));
        if (shiftMode != d->dragShiftMode_) {
            d->dragStartYaw_ = d->yaw_;
            d->dragStartPitch_ = d->pitch_;
            d->dragStartCenterPos_ = d->centerPos_;
            d->dragShiftMode_ = shiftMode;
            d->dragPos_ = e->pos();
        }
        float dx = e->pos().x() - d->dragPos_.x();
        float dy = e->pos().y() - d->dragPos_.y();
        QPoint diff = e->pos() - d->dragPos_;

        if (shiftMode) {
            QVector3D ddx = d->rightVector;
            QVector3D ddy = d->upVector;
            auto newPos = d->dragStartCenterPos_;
            newPos -= QVector2D(ddx.x(), ddx.y()) * (dx * d->sceneDef.viewWidth / d->image_->size().width());
            newPos -= QVector2D(ddy.x(), ddy.y()) * (dy / (1.f - ddy.z() * ddy.z())
                                                      * d->sceneDef.viewHeight / d->image_->size().height());
            newPos.setX(std::max(newPos.x(), 0.f));
            newPos.setY(std::max(newPos.y(), 0.f));
            newPos.setX(std::min<float>(newPos.x(), d->terrain_->size().width()));
            newPos.setY(std::min<float>(newPos.y(), d->terrain_->size().height()));
            d->centerPos_ = newPos;
        } else {
            d->yaw_ = d->dragStartYaw_ + diff.x() * 0.01f;
            d->pitch_ = std::max<float>(std::min<float>(d->dragStartPitch_ + diff.y() * 0.01f, M_PI * 0.499f), 0.1f);
        }

        update();
        e->accept();
    } else {
        emit clientMouseMoved(e);
    }
}

void TerrainView::wheelEvent(QWheelEvent *e)
{
    Q_D(TerrainView);

    float &scale = d->scale_;
    scale *= std::exp2f(e->angleDelta().y() * 0.004f);
    scale = std::min(scale, 100.f);
    scale = std::max(scale, .1f);

    update();
    e->accept();
}

void TerrainView::showOptionsWindow()
{
    if (!optionsWindow) {
        optionsWindow.reset(new TerrainViewOptionsWindow(this, this));
        connect(optionsWindow.data(), SIGNAL(destroyed()), SLOT(optionsWindowClosed()));
    }
    optionsWindow->show();
}

void TerrainView::terrainUpdate(const QRect &bounds)
{
    Q_D(TerrainView);
    d->terrainUpdate(bounds);
}

void TerrainView::optionsWindowClosed()
{
    if (optionsWindow)
        optionsWindow.take()->deleteLater();
}
