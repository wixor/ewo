#include "evolution.h"

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
