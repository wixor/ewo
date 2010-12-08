#include <queue>

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
    debug("difference job starting, bounding box (%d,%d) - (%d,%d), dx=%lf, dy=%lf, scale=%lf\n",
           x1,y1,x2,y2,dx,dy,scale);

    for(int y = y1; y<=y2; y++)
        for(int x = x1; x<=x2; x++)
            (*dst)[y][x] = evalAt(x,y);
}
    
/* ------------------------------------------------------------------------ */

Array2D<float> evaluateImage(const Image &src, int steps, const std::vector<float> &scales)
{
    int w = src.getWidth(), h = src.getHeight();
    
    float maxscale = 0;
    for(int s=0; s<(int)scales.size(); s++)
        maxscale = std::max(maxscale, scales[s]);

    int border = (int)ceilf(2.5f*maxscale);
    
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

/* ------------------------------------------------------------------------ */

inline bool onBoard(int x, int y, int W, int H) { return x>=0 && x<W && y>=0 && y<H; }

PoiFinder::PoiFinder(const Array2D<float> &src, int bounding = 1) //szukaj punktów napałowo
{
    const float R0 = 20.0;
    const float minimum = 20.0;
    
    H = src.getHeight(), W = src.getWidth();
    
    Array2D<char> tabu(W, H);
    memset(tabu[0], 0, W*H);
    
    std::priority_queue<pnt> best;
    
    for (int j=bounding; j<H-bounding; j++)
        for (int i=bounding; i<W-bounding; i++) 
            best.push(pnt(i,j,src[j][i]));
    
    while (true) //kolko skalujemy od R0 do 2*R0
    {
        if (best.empty()) break;
        pnt curr = best.top(); best.pop();
        if (tabu[curr.y][curr.x]) continue;
        if (curr.val < minimum) break;
        
        poi.push_back(curr);
        curr.val = std::max(std::min(curr.val, 0.0f), 255.0f);
        float R = R0*3.0 - 2*curr.val*R0/255.0f;
        int iR = R;
        for (int y=-iR; y<=iR; y++)
            for (int x=-iR; x<=iR; x++)
                if (x*x+y*y <= (int)R*R && onBoard(x+curr.x,y+curr.y,W,H))
                    tabu[curr.y+y][curr.x+x] = 1;
    }
}
    
Image PoiFinder::toImage(float threshold = 50.0, int howmany = 1000000000)
{
    Image ret(W, H);
    ret.fill(0);
    for (int i=0; i<(int)poi.size() && i<howmany && poi[i].val >= threshold; i++)
    {
        int x0 = poi[i].x, y0 = poi[i].y;
        for (int x=x0-1; x<=x0+1; x++)
            for (int y=y0-1; y<=y0+1; y++)
                if (onBoard(x,y,W,H))
                    ret[y][x] = 255;
    }
    fprintf(stderr, "to image done\n");
    return ret;
}

/* ------------------------------------------------------------------------ */


int main(int argc, char *argv[])
{
    if(argc < 5) {
        fprintf(stderr, "usage: poi [input] [output] [steps] [scale1, scale2, ...] \n");
        return 1;
    }

    spawn_worker_threads(2);

    int steps = strtol(argv[3],NULL,0);

    std::vector<float> scales;
    for(int i=4; i<argc; i++)
        scales.push_back(strtod(argv[i],NULL));

    Image src = Image::readPGM(argv[1]);
    Array2D<float> eval = evaluateImage(src, steps, scales);
    Image evalImg = reduceEvaluationToImage(eval);
    evalImg.writePGM(argv[2]);
    
    PoiFinder PF(eval, scales.back()/2+1);
    PF.toImage(40.0, 100).writePGM("foo.pgm");
    
    return 0;
}

/* 
for (int i=0, j=0, k=0; k<as+bs; k++)
{
    if (i == as) C[k] = B[j++];
    else if (j == bs) C[k] = A[i++];
    else if (A[i] < B[j]) C[k] = A[i++];
    else C[k] = B[j++];
}*/


