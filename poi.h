#ifndef __POI_H__
#define __POI_H__

#include <cmath>
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

    static inline float dist(const POI &a, const POI &b) {
        return sqrtf((a.x-b.x)*(a.x-b.x) + (a.y-b.y)*(a.y-b.y));
    }
};

Array2D<float> evaluateImage(const Image &src, int steps, const std::vector<float> &scales);
Image reduceEvaluationToImage(const Array2D<float> &eval);
std::vector<POI> findPOIs(const Array2D<float> &eval, float threshold, int count, float param);
std::vector<POI> findPOIs(const Array2D<float> &eval, float threshold, int count);
void overlayPOIs(const std::vector<POI> &pois, Image *dst);
Image renderPOIs(const std::vector<POI> &pois, int w, int h);

Array2D<float> gaussianBlur(const Image& img, float deviation, int border);
Image renderGaussianDifference(const Image& img1, const Image img2, float deviation, int border, bool smart);

#endif
