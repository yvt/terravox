#ifndef TERRAIN_H
#define TERRAIN_H

#include <QSize>
#include <QVector>
#include <QPoint>
#include <QRect>
#include <cmath>
#include <array>

class Terrain final
{
public:
    Terrain(const QSize &size);
    ~Terrain();

    const QSize &size() const { return size_; }
    float *landformData() { return landform_.data(); }
    quint32 *colorData() { return color_.data(); }

    float &landform(int x, int y)
    {
        Q_ASSERT(x >= 0);
        Q_ASSERT(y >= 0);
        Q_ASSERT(x < size_.width());
        Q_ASSERT(y < size_.height());
        return landform_[x + y * size_.width()];
    }
    quint32 &color(int x, int y)
    {
        Q_ASSERT(x >= 0);
        Q_ASSERT(y >= 0);
        Q_ASSERT(x < size_.width());
        Q_ASSERT(y < size_.height());
        return color_[x + y * size_.width()];
    }

    void copyFrom(Terrain *, QPoint destination, QRect src = QRect());

    void quantize();
    static inline float quantizeOne(float f)
    {
        return std::max(std::min(std::round(f), 63.f), 0.f);
    }

    static std::array<int, 3> expandColor(quint32 col)
    {
        return std::array<int, 3> {{int(col & 255), int((col >> 8) & 255), int((col >> 16) & 255)}};
    }

private:
    QSize size_;

    QVector<float> landform_;
    QVector<quint32> color_;
};

#endif // TERRAIN_H
