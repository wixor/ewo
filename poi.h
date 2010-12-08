#ifndef __POI_H__
#define __POI_H__

#include <vector>
#include "image.h"

struct POI {
    int x, y;
    float val;

    inline POI() { }
    inline POI(int x, int y, float v) : x(x), y(y), val(v) { }
    inline bool operator<(const POI &p) const {
        return val != p.val ? val > p.val :
                 x != p.x   ? x   < p.x :
                              y   < p.y;
    }
};

Array2D<float> evaluateImage(const Image &src, int steps, const std::vector<float> &scales);
Image reduceEvaluationToImage(const Array2D<float> &eval);
std::vector<POI> findPOIs(const Array2D<float> &eval, int border);
void renderPOIs(const std::vector<POI> &pois, Image *dst, float threshold, int count);

#endif
