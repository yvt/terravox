#ifndef VXL
#define VXL

#include <QByteArray>
class Terrain;

namespace vxl
{
    Terrain *load(QByteArray&, QString& retMessage);
    bool save(QByteArray&, Terrain *);
}

#endif // VXL

