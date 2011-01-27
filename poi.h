#ifndef __POI_H__
#define __POI_H__

#include <cmath>
#include <vector>

#include "util.h"
#include "image.h"

struct POI : public Point
{
    float val;

    inline POI() { }
    inline POI(float x, float y, float val) : Point(x,y), val(val) { }
    inline POI(const Point &p, float val) : Point(p), val(val) { }
    inline POI(const POI &p) : Point(p.x, p.y), val(p.val) { }

    inline bool operator<(const POI &p) const {
        return val != p.val ? val > p.val : (Point)(*this) < (Point)p;
    } 
};

inline POI operator*(const Matrix &M, const POI &p) {
    return POI(M * (Point)p, p.val);
}

inline bool compX(const POI &p, const POI &q) {
    return p.x != q.x ? p.x < q.x : p.y < q.y;
}

Array2D<float> evaluateImage(const Image &src, int steps, const std::vector<float> &scales);
Image reduceEvaluationToImage(const Array2D<float> &eval);
std::vector<POI> findPOIs(const Array2D<float> &eval, float threshold, int count, float param);
std::vector<POI> findPOIs(const Array2D<float> &eval, float threshold, int count);

#endif
