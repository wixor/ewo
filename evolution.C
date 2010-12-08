#include <cmath>
#include <vector>
#include "poi.h"
#include "evolution.h"

class Matrix33 : public Array2D<float>
{
public:
    inline Matrix33();
    virtual ~Matrix33();

    inline static Matrix33 translation(float dx, float dy);
    inline static Matrix33 rotation(float alpha);
    inline static Matrix33 scaling(float sx, float sy);
    
    inline POI operator*(const POI &p) const;
    inline Matrix33 operator*(const Matrix33 &M) const;
};

Matrix33::Matrix33() : Array2D<float>(3,3)
{
    Matrix33 &me = *this;
    me[0][0] = me[0][1] = me[0][2] = me[1][0] = me[1][1] = me[1][2] = me[2][0] = me[2][1] = me[2][2] = 0.0f;
}

Matrix33::~Matrix33()
{
    /* empty */
}

Matrix33 Matrix33::translation(float dx, float dy)
{
    Matrix33 ret;
    ret[0][0] = ret[1][1] = ret[2][2] = 1.0f;
    ret[0][2] = dx; ret[1][2] = dy;
    return ret;
}
Matrix33 Matrix33::rotation(float alpha)
{
    Matrix33 ret;
    ret[0][0] = ret[1][1] = cosf(alpha);
    ret[1][0] = -(ret[0][1] = sinf(alpha));
    ret[2][2] = 1.0f;
    return ret;
}
Matrix33 Matrix33::scaling(float sx, float sy)
{
    Matrix33 ret;
    ret[0][0] = sx; ret[1][1] = sy;
    ret[2][2] = 1.0f;
    return ret;
}

POI Matrix33::operator*(const POI &p) const
{
    const Matrix33 &me = *this;
    return POI(me[0][0]*p.x + me[0][1]*p.y + me[0][2], 
               me[1][0]*p.x + me[1][1]*p.y + me[1][2],
               p.val);
}

Matrix33 Matrix33::operator*(const Matrix33 &M) const
{
    const Matrix33 &me = *this;
    Matrix33 ret;
    for (int i=0; i<3; i++)
        for (int j=0; j<3; j++)
            ret[i][j] = me[i][0]*M[0][j] + me[i][1]*M[1][j] + me[i][2]*M[2][j];
    return ret;
}

/* ------------------------------------------------------------------------ */


/* instance of the image, that is an image (is it really necessary?) itself 
 * with a lot of additional things we managed to compute (POIs, mostly) */
class ImageInstance 
{
public:
    const Image *rawImg;
    std::vector<POI> poiVec;
    /* jakieś jeszcze rzeczy, typu mapka kolorów */

    inline ImageInstance(const ImageInstance &inst) {
        rawImg = inst.rawImg; //wskaznik ten sam, bo i obrazek ten sam
        poiVec = inst.poiVec;
    }
        
    inline ImageInstance(const Image& img) {
        rawImg = &img;
        /* tu powinniśmy obliczać poiImg oraz poiVec */
    }
    
    float distance(const ImageInstance& inst);
};

/* ------------------------------------------------------------------------ */

class Individual 
{
public:
    float value;

    inline Individual() : value(-1) { }
    virtual Matrix33 toMatrix() const = 0;
};

class Agent : public Individual
{
    float alfa;
    float dx, dy;
    float scx, scy;

public:
    virtual Matrix33 toMatrix();

    std::vector<POI> applyToPOIs(const std::vector<POI> &v);
    ImageInstance apply(const ImageInstance& inst);
};

Matrix33 Agent::toMatrix()
{
    return Matrix33::scaling(scx, scy) *
           Matrix33::translation(dx, dy) *
           Matrix33::rotation(alfa);
}

std::vector<POI> Agent::applyToPOIs(const std::vector<POI> &v)
{
    Matrix33 M = toMatrix();

    std::vector<POI> ret(v.size());
    for (int i=0; i<v.size(); i++)
        ret[i] = M*v[i];
    return ret;
}

ImageInstance Agent::apply(const ImageInstance& inst)
{
    /* jezeli uzyjesz tutaj konstruktora kopiujacego, to najpierw
     * skopiujesz w nim vector POI, a pozniej bedziesz go nadpisywal... */
}

