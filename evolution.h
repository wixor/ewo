#ifndef __EVOLUTION
#define __EVOLUTION
#include "mpoi.h"
#include "workers.h"
#include "image.h"
#include <cmath>
#include <vector>

class Matrix33 : public Array2D<float>
{public:
    inline float &tab(int i, int j) { return (*this)[i][j]; }
    inline Matrix33() : Array2D<float>(3,3) {
        tab(0,0) = tab(0,1) = tab(0,2) = tab(1,0) = tab(1,1) = tab(1,2) = tab(2,0) = tab(2,1) = tab(2,2) = 0.0;
    }
    inline pnt operator*(pnt p) //mnożymy przez wektor (p.x, p.y, 1)
    {
        return pnt(
            tab(0,0)*p.x+tab(0,1)*p.y+tab(0,2), 
            tab(1,0)*p.x+tab(1,1)*p.y+tab(1,2)
        );
    }
    inline Matrix33 operator*(const Matrix33& M)
    {
        Matrix33 Ret;
        for (int i=0; i<3; i++)
            for (int j=0; j<3; j++)
                Ret[i][j] = tab(i,0)*M[0][j] + tab(i,1)*M[1][j] + tab(i,2)*M[2][j];
        return Ret;
    }
};

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

/* ******************************************************************************* */
/* ******************************************************************************* */

class ImageInstance 
/* instance of the image, that is an image (is it really necessary?) itself 
with a lot of additional things we managed to compute (POIs, mostly) */
{public:
    const Image *rawImg;
    std::vector<pnt> poiVec;
    /* jakieś jeszcze rzeczy, typu mapka kolorów */
    inline ImageInstance(const ImageInstance &inst) {
        rawImg = inst.rawImg; //wskaznik ten sam, bo i obrazek ten sam
        poiVec = inst.poiVec;
    }
        
    inline ImageInstance(const Image& img) {
        rawImg = &img;
        /* tu powinniśmy obliczać poiImg oraz poiVec */
    }
    
    float distance(const ImageInstance& inst) ;
};

/* ******************************************************************************* */

class Individual //virtual
{
    float val;
public:
    inline Individual() { val = -1.0; }
    Matrix33 toMatrix();
    inline float &value() { return val; }
    inline const float &value() const { return val; }
};

class Agent : public Individual
{
    float alfa;
    float dx, dy;
    float scx, scy;
public:
    Matrix33 toMatrix();
    std::vector<pnt> applyToVector(const std::vector<pnt>& v);
    ImageInstance apply(const ImageInstance& inst);
};

/* ******************************************************************************* */

class EVOLUTION
{
    const ImageInstance *img;
    const ImageInstance *pretend;
    
    
    
};
    
    
    
    
    
    




#endif
    