#include "vxl.h"
#include "terrain.h"
#include <QScopedPointer>
#include <QString>

namespace vxl
{

    Terrain *load(QByteArray &bytes, QString &retMessage)
    {
        QScopedPointer<Terrain> terrain(new Terrain(QSize(512, 512)));

        retMessage = "";

        const auto *v = reinterpret_cast<const quint8 *>(bytes.data());
        const auto *end = v + bytes.size();
        bool nonHeightField = false;

        for (int y = 0; y < 512; ++y) {
            for (int x = 0; x < 512; ++x) {
                if (v + 2 >= end) {
                    goto malformat;
                }
                int surfZ = v[1];
                quint32 surfColor = 0x1000000;
                for(int z = v[1]; z <= v[2]; ++z) {
                    if (v + (z - v[1] + 1) * 4 + 4 > end) {
                        goto malformat;
                    }
                    auto col = *reinterpret_cast<const quint32*>(v + (z - v[1] + 1) * 4);
                    col &= 0xffffff;
                    if (surfColor == 0x1000000)
                        surfColor = col;
                }
                if (v[0]) {
                    nonHeightField = true;
                    do {
                        if (v >= end) {
                            goto malformat;
                        }
                        v += (int)v[0] * 4;
                    } while(v[0]);
                }
                if (v + 2 >= end) {
                    goto malformat;
                }
                v += ((int)v[2] - (int)v[1] + 2) * 4;

                if (surfZ >= 64) {
                    goto malformat;
                }
                terrain->landform(x, y) = surfZ;
                terrain->color(x, y) = surfColor;
            }
        }

        if (nonHeightField) {
            retMessage = "Terrain cannot be represented as a height field. Hollow area was removed.";
        }

        return terrain.take();

    malformat:
        retMessage = "Invalid VXL file.";
        return nullptr;
    }

    bool save(QByteArray &bytes, Terrain *t)
    {
        Q_ASSERT(t);
        auto size = t->size();
        for (int y = 0; y < size.height(); ++y) {
            for (int x = 0; x < size.width(); ++x) {
                int surfZ = static_cast<int>(t->landform(x, y));
                quint32 color = t->color(x, y);

                int aroundZ = surfZ;
                if (x > 0) aroundZ = std::max(aroundZ, static_cast<int>(t->landform(x - 1, y)));
                if (y > 0) aroundZ = std::max(aroundZ, static_cast<int>(t->landform(x, y - 1)));
                if (x < size.width() - 1) aroundZ = std::max(aroundZ, static_cast<int>(t->landform(x + 1, y)));
                if (y < size.height() - 1) aroundZ = std::max(aroundZ, static_cast<int>(t->landform(x, y + 1)));
                int colorEnd = std::max(aroundZ, surfZ) + 1;
                bytes.append((char)0);
                bytes.append((char)surfZ);
                bytes.append((char)colorEnd - 1);
                bytes.append((char)0);
                for (int z = surfZ; z < colorEnd; ++z)
                    bytes.append(reinterpret_cast<char *>(&color), 4);
            }
        }
        return true;
    }

}
