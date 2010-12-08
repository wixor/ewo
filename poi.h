#ifndef __POI
#define __POI
#include <cstdlib>
#include <cmath>
#include <cassert>
#include <unistd.h>
#include <algorithm>
#include <vector>

#include "image.h"
#include "workers.h"

#define debug(fmt, ...) fprintf(stderr, fmt, ## __VA_ARGS__)

struct pnt {
    int x, y;
    float val; // zakładam, że val jest 0 - 255
    pnt(int x = -1, int y = -1, float v = 0) : x(x), y(y), val(v) {}
    inline bool operator<(const pnt& p) const { return val==p.val ? std::make_pair(x,y) < std::make_pair(p.x,p.y) : val < p.val; }
};


Array2D<float> evaluateImage(const Image &src, int steps, const std::vector<float> &scales);
Image reduceEvaluationToImage(const Array2D<float> &eval);

class PoiFinder
{
    std::vector<pnt> poi;
    int W, H;
public:
    PoiFinder(const Array2D<float> &src, int bounding); //szukaj punktów napałowo
    inline std::vector<pnt> getPoiVec() const { return poi; }
    Image toImage(float threshold, int howmany);
};


#endif