#ifndef __POI_H__
#define __POI_H__

#include <cmath>
#include <vector>
#include "image.h"

Array2D<float> evaluateImage(const Image &src, int steps, const std::vector<float> &scales);
Image reduceEvaluationToImage(const Array2D<float> &eval);
std::vector<POI> findPOIs(const Array2D<float> &eval, float threshold, int count, float param);
std::vector<POI> findPOIs(const Array2D<float> &eval, float threshold, int count);
void overlayPOIs(const std::vector<POI> &pois, Image *dst);
Image renderPOIs(const std::vector<POI> &pois, int w, int h);

Array2D<float> gaussianBlur(const Image& img, float deviation, int border);
Image renderGaussianDifference(const Image& img1, const Image img2, float deviation, int border, bool smart);

#endif
