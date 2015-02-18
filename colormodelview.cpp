#include "colormodelview.h"
#include <QPainter>
#include <QMouseEvent>
#include <xmmintrin.h>
#include <QLinearGradient>
#include <array>

ColorModelView::ColorModelView(QWidget *parent) :
    QWidget(parent),
    drag_(DragMode::None)
{

}

ColorModelView::~ColorModelView()
{

}

void ColorModelView::setValue(const QColor &col)
{
    if (col == value_) {
        return;
    }

    if (col.hue() != value_.hue()) {
        // invalidate color model image
        mainImage_ = QPixmap();
    }

    value_ = col;


    update();
    emit valueChanged(col);
}

void ColorModelView::paintEvent(QPaintEvent *)
{
    QPainter p(this);

    auto mainBounds = mainAreaBounds();
    auto sideBounds = sideAreaBounds();

    if (mainImage_.isNull()) {
        // FIXME: support other color model?
        QImage img(256, 256, QImage::Format_RGB32);
        auto *pixels = reinterpret_cast<quint32 *>(img.bits());
        auto basecolor = QColor::fromHsv(value_.hsvHue(), 255, 255);
        auto basecolorMM = _mm_setr_epi32(basecolor.blue(), basecolor.green(), basecolor.red(), 0);
        basecolorMM = _mm_add_epi32(basecolorMM, _mm_srli_epi32(basecolorMM, 7)); // map [0, 255] to [0, 256]
        auto white = _mm_set1_epi32(256 * 255);
        auto dX = _mm_sub_epi32(basecolorMM, _mm_set1_epi32(256));
        for (int y = 0; y < 256; ++y) {
            auto brightness = _mm_set1_epi32(256 - y - (y >> 7));
            auto col = white; // [0, 256 * 255]
            for (int x = 0; x < 256; ++x) {
                auto c = _mm_mullo_epi16(_mm_srli_epi32(col, 8), brightness);
                c = _mm_srli_epi16(c, 8); // [0, 255]
                c = _mm_packs_epi32(c, c);
                c = _mm_packus_epi16(c, c);

                _mm_store_ss(reinterpret_cast<float *>(&pixels[x + y * 256]),
                        _mm_castsi128_ps(c));

                col = _mm_add_epi32(col, dX);
            }
        }
        mainImage_ = QPixmap::fromImage(img);
    }
    if (sideImage_.isNull()) {
        // FIXME: support other color model?
        QImage img(1, 360, QImage::Format_RGB32);
        auto *pixels = reinterpret_cast<quint32 *>(img.bits());
        for (int i = 0; i < 360; ++i) {
            pixels[i] = QColor::fromHsv(i, 255, 255).rgb();
        }
        sideImage_ = QPixmap::fromImage(img);
    }

    // draw borders
    p.setRenderHint(QPainter::Antialiasing);
    QLinearGradient grad(mainBounds.x(), mainBounds.top(),
                         mainBounds.x(), mainBounds.bottom());
    grad.setColorAt(0, QColor(255, 255, 255, 4));
    grad.setColorAt(1, QColor(255, 255, 255, 13));

    p.fillRect(mainBounds.x() - 2, mainBounds.y() - 2,
               mainBounds.width() + 4, mainBounds.height() + 4, grad);
    p.fillRect(sideBounds.x() - 2, sideBounds.y() - 2,
               sideBounds.width() + 4, sideBounds.height() + 4, grad);

    QColor shadowColor = QColor(0, 0, 0, 80);
    p.fillRect(mainBounds.x() - 1, mainBounds.y() - 1,
               mainBounds.width() + 2, mainBounds.height() + 2, shadowColor);
    p.fillRect(sideBounds.x() - 1, sideBounds.y() - 1,
               sideBounds.width() + 2, sideBounds.height() + 2, shadowColor);


    p.drawPixmap(mainBounds, mainImage_);
    p.drawPixmap(sideBounds, sideImage_);

    // draw selected value
    QPointF pt(mainBounds.x() + mainBounds.width() * value_.saturation() / 256.f,
               mainBounds.bottom() - mainBounds.height() * value_.value() / 256.f);
    p.setBrush(QColor(0, 0, 0)); p.setPen(QColor(0, 0, 0, 0));
    p.drawEllipse(pt, 4.f, 4.f);
    p.setBrush(QColor(255, 255, 255));
    p.drawEllipse(pt, 3.f, 3.f);
    p.setBrush(value_);
    p.drawEllipse(pt, 2.f, 2.f);

    QRectF rt(sideBounds.x(), sideBounds.y() + sideBounds.height() * value_.hue() / 360.f,
              sideBounds.width(), 0.f);
    p.setBrush(QColor(255, 255, 255));
    p.drawPolygon(std::array<QPointF, 3> {
        QPointF(rt.left() - 4.f, rt.top() - 3.f),
        QPointF(rt.left() - 4.f, rt.top() + 3.f),
        QPointF(rt.left() - 1.f, rt.top())
    } .data(), 3);
    p.drawPolygon(std::array<QPointF, 3> {
        QPointF(rt.right() + 4.f, rt.top() - 3.f),
        QPointF(rt.right() + 4.f, rt.top() + 3.f),
        QPointF(rt.right() + 1.f, rt.top())
    } .data(), 3);
}

void ColorModelView::mousePressEvent(QMouseEvent *e)
{
    if (e->button() == Qt::LeftButton) {
        auto p = e->pos();
        if (mainAreaBounds().contains(p)) {
            drag_ = DragMode::Main;
        } else if (sideAreaBounds().contains(p)) {
            drag_ = DragMode::Side;
        }
        mouseMoveEvent(e);
    }
}

void ColorModelView::mouseReleaseEvent(QMouseEvent *e)
{
    if (e->button() == Qt::LeftButton) {
        drag_ = DragMode::None;
    }
}

void ColorModelView::mouseMoveEvent(QMouseEvent *e)
{
    if (drag_ == DragMode::Main) {
        auto bounds = mainAreaBounds();
        int sat = (e->x() - bounds.left()) * 256 / bounds.width();
        int val = 255 - (e->y() - bounds.top()) * 256 / bounds.height();
        sat = std::max(std::min(sat, 255), 0);
        val = std::max(std::min(val, 255), 0);
        setValue(QColor::fromHsv(value_.hsvHue(), sat, val));
    } else if (drag_ == DragMode::Side) {
        auto bounds = sideAreaBounds();
        int hue = (e->y() - bounds.top()) * 360 / bounds.height();
        hue = std::max(std::min(hue, 359), 0);
        setValue(QColor::fromHsv(hue, value_.hsvSaturation(), value_.value()));
    }
}

QRect ColorModelView::mainAreaBounds()
{
    auto s = size();
    int margin = 4;
    return QRect(margin, margin, s.width() - 17 - margin  * 2, s.height() - margin * 2);
}

QRect ColorModelView::sideAreaBounds()
{
    auto s = size();
    auto r = mainAreaBounds();
    int margin = 4;
    int spacing = 6;
    return QRect(r.right() + 1 + spacing, margin,
                 s.width() - (r.right() + 1 + spacing) - margin, r.height());
}

