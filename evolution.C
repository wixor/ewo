#include "evolution.h"

class Rotation : public Matrix33
{public:
    inline Rotation(float angle) : Matrix33() {
        tab(2,2) = 1;
        tab(0,0) = tab(1,1) = cos(angle);
        tab(0,1) = sin(angle);
        tab(1,0) = -sin(angle);
    }
};

class Translation : public Matrix33
{public:
    inline Translation(float dx, float dy) : Matrix33() {
        tab(0,0) = tab(1,1) = tab(2,2) = 1.0;
        tab(0,2) = dx;
        tab(1,2) = dy;
    }
};

class Scaling : public Matrix33
{public:
    inline Scaling(float scx, float scy) : Matrix33() {
        tab(0,0) = scx;
        tab(1,1) = scy;
        tab(2,2) = 1.0;
    }
};

/* ********************************************************************* */
/* ********************************************************************* */

Matrix33 Agent::toMatrix()
{
    return Scaling(scx, scy)*Translation(dx, dy)*Rotation(alfa);
}

std::vector<pnt> Agent::applyToVector(const std::vector<pnt>& v)
{
    Matrix33 M = toMatrix();
    std::vector<pnt> ret(v.size());
    for (int i=0; i<(int) v.size(); i++) ret[i] = M*v[i];
    return ret;
}

ImageInstance Agent::apply(const ImageInstance& inst)
{
    ImageInstance ret(inst);
    ret.poiVec = applyToVector(inst.poiVec);
    return ret;
}

int main()
{
    Agent A;
    return 0;
}
