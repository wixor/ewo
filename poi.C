#include <cstdio>
#include <cassert>
#include <cmath>
#include <vector>
#include <algorithm>
#include <stdexcept>


#include "workers.h"
#include "image.h"
#include "poi.h"

#define debug(fmt, ...) fprintf(stderr, fmt, ## __VA_ARGS__)

/* ------------------------------------------------------------------------ */

class PrefixSums : public Array2D<int>
{
public:
    PrefixSums(const Image &source);
    virtual ~PrefixSums();
    inline int query(int x1, int x2, int y1, int y2) const;
};

PrefixSums::PrefixSums(const Image &source)
    : Array2D<int>(source.getWidth()+1, source.getHeight()+1)
{
    PrefixSums &me = *this;
    for (int y=0; y<height; y++)
        for (int x=0; x<width; x++)
            me[y][x] = !y || !x ? 0 : source[y-1][x-1] + me[y-1][x] + me[y][x-1] - me[y-1][x-1];
}

PrefixSums::~PrefixSums()
{
    /* empty */
}

int PrefixSums::query(int x1, int x2, int y1, int y2) const
{
    if(x1 > x2) std::swap(x1, x2);
    if(y1 > y2) std::swap(y1, y2);

    assert(0 <= x1 && x1 < width);
    assert(0 <= x2 && x2 < width);
    assert(0 <= y1 && y1 < height);
    assert(0 <= y2 && y2 < height);

    const PrefixSums &me = *this;
    return me[y2][x2] - me[y2][x1] - me[y1][x2] + me[y1][x1];
}

/* ------------------------------------------------------------------------ */

class DifferenceJob : public AsyncJob
{
    inline float areaSum(float x1, float x2, float y1, float y2) const;
    inline float areaAvg(float x1, float x2, float y1, float y2) const;
    inline float evalAt(int x, int y) const;

public:
    const PrefixSums *src;
    Array2D<float> *dst;

    float dx, dy, scale;
    int x1,x2, y1,y2;

    virtual ~DifferenceJob();
    virtual void run();
};

DifferenceJob::~DifferenceJob()
{
    /* empty */
}

float DifferenceJob::areaSum(float x1, float x2, float y1, float y2) const
{
    if (x1>x2) std::swap(x1,x2);
    if (y1>y2) std::swap(y1,y2);

    float ix1 = ceilf(x1), iy1 = ceilf(y1),
          ix2 = floorf(x2), iy2 = floorf(y2);
    
    return
        (float)src->query(ix1, ix2, iy1, iy2)
        
        + (ix1-x1) * src->query(ix1-1, ix1, iy1, iy2)
        + (x2-ix2) * src->query(ix2, ix2+1, iy1, iy2)
        + (iy1-y1) * src->query(ix1, ix2, iy1-1, iy1)
        + (y2-iy2) * src->query(ix1, ix2, iy2, iy2+1)
        
        + (ix1-x1)*(iy1-y1) * src->query(ix1-1, ix1, iy1-1, iy1)
        + (x2-ix2)*(iy1-y1) * src->query(ix2, ix2+1, iy1-1, iy1)
        + (ix1-x1)*(y2-iy2) * src->query(ix1-1, ix1, iy2, iy2+1)
        + (x2-ix2)*(y2-iy2) * src->query(ix2, ix2+1, iy2, iy2+1);
}

float DifferenceJob::areaAvg(float x1, float x2, float y1, float y2) const
{
    return areaSum(x1,x2,y1,y2) / (fabs(x1-x2) * fabs(y1-y2));
}

float DifferenceJob::evalAt(int x, int y) const
{
    float x1 = 0.5f + x - 0.5f*scale, x2 = 0.5 + x + 0.5f*scale,
          y1 = 0.5f + y - 0.5f*scale, y2 = 0.5 + y + 0.5f*scale;

    return   areaAvg(x1,   x2,    y1,   y2)
           - areaAvg(x1+dx,x2+dx, y1+dy,y2+dy);
}

void DifferenceJob::run()
{
    /* debug("difference job starting, bounding box (%d,%d) - (%d,%d), dx=%lf, dy=%lf, scale=%lf\n",
           x1,y1,x2,y2,dx,dy,scale); <- chyba juz niepotrzebne */

    for(int y = y1; y<=y2; y++)
        for(int x = x1; x<=x2; x++)
            (*dst)[y][x] = evalAt(x,y);
}
    
/* ------------------------------------------------------------------------ */

static int getBorderSize(const std::vector<float> &scales)
{
    float maxscale = 0;
    for(int s=0; s<(int)scales.size(); s++)
        maxscale = std::max(maxscale, scales[s]);

    return (int)ceilf(2.5f*maxscale);
}

Array2D<float> evaluateImage(const Image &src, int steps, const std::vector<float> &scales)
{
    int w = src.getWidth(), h = src.getHeight();
    
    int border = getBorderSize(scales);
    
    std::vector<Array2D<float> > globalProfile(steps, Array2D<float>(w,h));
    std::vector<Array2D<float> > currentProfile(steps, Array2D<float>(w,h));

    for(int i=0; i<steps; i++)
        for(int y=0; y<h; y++)
            for(int x=0; x<w; x++)
                globalProfile[i][y][x] = 1.0f;

    PrefixSums ps(src);
    DifferenceJob jobs[steps];
    Completion c;

    for(int i=0; i<steps; i++) {
        jobs[i].src = &ps;
        jobs[i].dst = &currentProfile[i];
        jobs[i].x1 = border;
        jobs[i].y1 = border;
        jobs[i].x2 = w-border;
        jobs[i].y2 = h-border;
        jobs[i].completion = &c;
    }
    
    for(int s=0; s<(int)scales.size(); s++)
    {
        for(int i=0; i<steps; i++)
        {
            float angle = 2.0f*M_PI/steps*i;
            jobs[i].scale = scales[s];
            jobs[i].dx = jobs[i].scale * cosf(angle);
            jobs[i].dy = jobs[i].scale * sinf(angle);
            aq->queue(&jobs[i]);
        }
        
        c.wait();
        
        for(int i=0; i<steps; i++)
            for(int y=0; y<h; y++)
                for(int x=0; x<w; x++)
                    globalProfile[i][y][x] *= currentProfile[i][y][x];
    }
    
    Array2D<float> ret(w,h);
    for(int y=0; y<h; y++)
        for(int x=0; x<w; x++)
            if(x < border || x > w-border ||
               y < border || y > h-border)
                ret[y][x] = 0;
            else
            {
                float min = 1e+10f, max=1e-10f;
                for(int i=0; i<steps; i++)
                    min = std::min(min, globalProfile[i][y][x]),
                    max = std::max(max, globalProfile[i][y][x]);

                ret[y][x] = powf(max-min, 1.0f/scales.size());
            }

    return ret;
}

Image reduceEvaluationToImage(const Array2D<float> &eval)
{
    int w = eval.getWidth(), h = eval.getHeight();

    float max = 0;
    for(int y=0; y<h; y++)
        for(int x=0; x<w; x++)
            max = std::max(max, eval[y][x]);

    Image ret(w,h);
    for(int y=0; y<h; y++)
        for(int x=0; x<w; x++)
            ret[y][x] = std::min(255, std::max(1, (int)round(eval[y][x]/max*255)));

    return ret;
}

std::vector<POI> findPOIs(const std::vector<POI> &all, int W, int H, float threshold, int count, float tabuSizeParam)
{
    Image tabu(W, H);
    tabu.fill(0);
    std::vector<POI> pois;

    for(int i=0; i<(int)all.size() && (int)pois.size() < count; i++)
    {
        const POI &curr = all[i];
        
        if(tabu[curr.y][curr.x]) continue;
        if(curr.val < threshold) break;
        
        pois.push_back(curr);
        
        float R = tabuSizeParam*(W+H)/curr.val;
        int iR = R;

        for (int y=-iR; y<=iR; y++)
            for (int x=-iR; x<=iR; x++)
                if (x*x+y*y <= (int)R*R && tabu.inside(x+curr.x,y+curr.y))
                    tabu[curr.y+y][curr.x+x] = 1;
    }

    return pois;
}
    
std::vector<POI> findPOIs(const Array2D<float> &eval, float threshold, int count)
{
    return findPOIs(eval, threshold, count, 0);
}
std::vector<POI> findPOIs(const Array2D<float> &eval, float threshold, int count, float tabuSizeParam)
{
    int w = eval.getWidth(), h = eval.getHeight();

    std::vector<POI> all;
    for(int y=0; y<h; y++)
        for(int x=0; x<w; x++)
            all.push_back(POI(x,y,eval[y][x]));

    std::sort(all.begin(), all.end());
    
    if (tabuSizeParam != 0.0f) return findPOIs(all, w, h, threshold, count, tabuSizeParam);
    
    float downval = 0.1f, upval = 8.0f, midval;
    std::vector<POI> ret;
    while (upval-downval > 0.1f)
    {
        midval = (upval+downval)/2.0f;
        ret = findPOIs(all, w, h, threshold, count*4/3, midval);
        if ((int)ret.size() == count) break;
        if ((int)ret.size() > count) //za duzo punktow, wiec za male tabu
            downval = midval;
        else
            upval = midval;
    }
    debug("found %d pois (looking for %d). threshold = %f, tabuParam = %f\n", ret.size(), count, threshold, midval);
    
    return ret;
}
    
void overlayPOIs(const std::vector<POI> &pois, Image *dst)
{
    for (int i=0; i<(int)pois.size(); i++)
        dst->drawRect(pois[i].x-1, pois[i].x+1, pois[i].y-1, pois[i].y+1, 255);
}

Image renderPOIs(const std::vector<POI> &pois, int w, int h)
{
    Image ret(w,h);
    ret.fill(1);
    overlayPOIs(pois,&ret);
    return ret;
}

/* ------------------------------------------------------------------------ */

class GaussianJob : public AsyncJob
{
public:
    int W, H, borderW;
    float deviation;
    const Image *src, *imgSrc2;
    Array2D<float> *imgDest;
    int fromY, toY;
    
    virtual ~GaussianJob();
    virtual void run();
    GaussianJob(int w, int h, int border, float deviation, const Image* src, Array2D<float> *dest) : 
        W(w), H(h), borderW(border), deviation(deviation), src(src), imgDest(dest) {}
    GaussianJob() {}
};

GaussianJob::~GaussianJob() { /* empty */ }

void GaussianJob::run()
{
    debug("running %d %d\n", fromY, toY);
    const int R = 1+(int)2.0f*deviation; //regula 2 sigm :D
    
    float sumweight = 0.0f;
    float distr[2*R+1][2*R+1];
    for (int dx=-R; dx<=R; dx++)
        for (int dy=-R; dy<=R; dy++) {
            distr[R+dy][R+dx] = expf(-(float)(dx*dx)/(2.0f*deviation*deviation)) *
                                expf(-(float)(dy*dy)/(2.0f*deviation*deviation));
            // distr[R+dy][R+dx] = 1.0f/(2*R+1)/(2*R+1);
            sumweight += distr[R+dy][R+dx];
        }
    
    debug("sumweight = %f\n", sumweight); //2*pi*dev?
    
    for (int y=std::max(R,fromY); y<std::min(toY,H-R); y++)
        for (int x=std::max(R,borderW); x<std::min(W-R, W-borderW); x++)
        {
            float sumval = 0.0f;
            for (int dx=-R; dx<=R; dx++)
                for (int dy=-R; dy<=R; dy++) {
                        const float &f = distr[R+dy][R+dx];
                        sumval += f * (*src)[y+dy][x+dx];
                    }
            (*imgDest)[y][x] = sumval/sumweight;
            // debug("y = %d, x = %d. sumw = %f, val = %f\n", y, x, sumweight, (*imgDest)[y][x]);
        }
    debug("done %d %d\n", fromY, toY);
}

Array2D<float> gaussianBlur(const Image& img, float deviation, int border)
{
    int H = img.getHeight(), W = img.getWidth();
    Array2D<float> retimg(W, H);
    
    const int JobSlice = 50;
    int nJobs = 1+(H-2*border)/JobSlice;
    GaussianJob GJ[nJobs];
    Completion C;
    for (int i=0; i<nJobs; i++)
    {
        GJ[i] = GaussianJob(W, H, border, deviation, &img, &retimg);
        GJ[i].completion = &C;
        GJ[i].fromY = border + i*JobSlice;
        GJ[i].toY = std::min(H-border, border+(i+1)*JobSlice);
        debug("queuing %d %d\n", GJ[i].fromY, GJ[i].toY);
        aq->queue(&GJ[i]);
    }
    C.wait();
    
    return retimg;
}

Image renderGaussianDifference(const Image& img1, const Image img2, float deviation, int border, bool smart)
{
    int H1 = img1.getHeight(), W1 = img1.getWidth(),
        H2 = img2.getHeight(), W2 = img2.getWidth();
        
    if (W1 != W2 || H1 != H2)
        fprintf(stderr, "WARNING: wrong image dimensions (difference gauss), truncating to smaller\n");
    
    int W = std::min(W1, W2), H = std::min(H1,H2);
    
    Array2D<float> eval1 = gaussianBlur(img1, deviation, border);
    debug("blur 1 has been done\n");
    Array2D<float> eval2 = gaussianBlur(img2, deviation, border);
    debug("blur 2 has been done\n");
    
    Image ret(W, H);
    for (int y=0; y<H; y++)
        for (int x=0; x<W; x++) {
            if (!smart)
                ret[y][x] = std::max(1, std::min(255, (int)abs(eval1[y][x]-eval2[y][x])));
            else {
                ret[y][x] = 255;
                for (int dx=-deviation-1; dx<=deviation+1; dx++)
                    for (int dy=-deviation-1; dy<=deviation+1; dy++) 
                        if (ret.inside(x+dx, y+dy)) {
                            ret[y][x] = (uint8_t)std::min((int)ret[y][x],
                                            std::max(1, std::min(255, (int)abs(eval1[y][x]-eval2[y+dy][x+dx])))/2 +
                                            std::max(1, std::min(255, (int)abs(eval2[y][x]-eval1[y+dy][x+dx])))/2);
                        }
            }
        }
    return ret;
}

// Image renderSmartGaussian(const Image& img1, const Image& img2, float deviation, int border)


/*
int main(int argc, char *argv[])
{
    if(argc < 5) {
        fprintf(stderr, "usage: poi [input] [steps] [scale1, scale2, ...] \n");
        return 1;
    }

    spawn_worker_threads(2);

    int steps = strtol(argv[2],NULL,0);

    std::vector<float> scales;
    for(int i=3; i<argc; i++)
        scales.push_back(strtod(argv[i],NULL));

    Image src = Image::readPGM(argv[1]);
    Array2D<float> eval = evaluateImage(src, steps, scales);
    
    reduceEvaluationToImage(eval).writePGM("eval.pgm");
    
    std::vector<POI> pois = findPOIs(eval, getBorderSize(scales));
    printf("found %d pois\n", pois.size());

    Image poisImg(src.getWidth(), src.getHeight());
    poisImg.fill(0);
    renderPOIs(pois, &poisImg, 40.0, 100);
    poisImg.writePGM("pois.pgm");
    
    return 0;
}*/
