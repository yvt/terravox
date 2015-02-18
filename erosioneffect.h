#ifndef EROSIONEFFECT_H
#define EROSIONEFFECT_H

#include <QObject>

class Terrain;
class TerrainEdit;

struct ErosionParameters
{
    float density;
    float strength;
};

class ErosionEffect : public QObject
{
    Q_OBJECT
public:
    explicit ErosionEffect(QObject *parent = 0);
    ~ErosionEffect();

    void apply(Terrain *, TerrainEdit *);

    const ErosionParameters &parameters() const { return parameters_; }

signals:
    void parameterChanged(const ErosionParameters&);

public slots:
    void setParameters(const ErosionParameters&);

private:
    ErosionParameters parameters_;
};

#endif // EROSIONEFFECT_H
