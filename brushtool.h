#ifndef BRUSHTOOL_H
#define BRUSHTOOL_H

#include <QObject>
#include <QSharedPointer>
#include <QPoint>

enum class BrushType
{
    Raise,
    Lower,
    Paint,
    Smoothen,
    Blur
};

enum class BrushTipType
{
    Sphere,
    Cylinder,
    Square,
    Cone,
    Bell,
    Mountains
};

enum class BrushPressureMode
{
    AirBrush,
    Constant,
    Adjustable
};

class Terrain;
class TerrainEdit;
class BrushToolEdit;

struct BrushToolParameters
{
    BrushTipType tipType;
    int size;
    float strength;
    BrushPressureMode pressureMode;
    quint32 color;

    // "Mountains" parameters
    float scale;
    quint32 seed;

    void randomize();
};

class BrushTool : public QObject
{
    friend class BrushToolEdit;

    Q_OBJECT
public:
    explicit BrushTool(BrushType, QObject *parent = 0);
    ~BrushTool();

    void setParameters(const BrushToolParameters&);
    const BrushToolParameters &parameters() const { return parameters_; }

    QSharedPointer<Terrain> tip(QPoint pt = QPoint(-1000, -1000));

    BrushToolEdit *edit(TerrainEdit *, Terrain *);

    BrushType type() const { return type_; }

signals:
    void parameterChanged();

private:
    BrushToolParameters parameters_;
    QSharedPointer<Terrain> tip_;
    QPoint lastTipOrigin_;
    BrushType type_;

    QSharedPointer<Terrain> before_; // used during edit to hold old terrain


private slots:
    void discardTip();
};

class BrushToolEdit
{
    friend class BrushTool;
    BrushTool *tool;
    TerrainEdit *edit;
    Terrain *terrain;
    BrushToolParameters params;

    BrushToolEdit(TerrainEdit *, Terrain *, BrushTool *);

    template <class Map>
    void drawRaiseLower(const QPoint&, Map &&map);

    template <class Map>
    void drawColor(const QPoint&, Map &&map);

    void drawBlur(const QPoint&, float amount);
    void drawSmoothen(const QPoint&, float amount);
public:
    ~BrushToolEdit();

    void draw(const QPoint&, float strength);

    TerrainEdit *currentEdit() const { return edit; }
    const BrushToolParameters& parameters() const { return params; }
};

#endif // BRUSHTOOL_H
