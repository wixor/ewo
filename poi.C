#include <cstdio>
#include <cassert>
#include <cmath>
#include <vector>
#include <queue>
#include <algorithm>
#include <stdexcept>


#include "util.h"
#include "poi.h"
#include "image.h"

/* ------------------------------------------------------------------------ */

class PrefixSums : public Array2D<int>
{
public:
    PrefixSums(const Image &source);
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

int PrefixSums::query(int x1, int x2, int y1, int y2) const
{
    if(x1 > x2) std::swap(x1, x2);
    if(y1 > y2) std::swap(y1, y2);

/*  use only for debugging - hurts performance much!  
    assert(0 <= x1 && x1 < width);
    assert(0 <= x2 && x2 < width);
    assert(0 <= y1 && y1 < height);
    assert(0 <= y2 && y2 < height); */

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
    for(int y = y1; y < y2; y++)
        for(int x = x1; x < x2; x++)
            (*dst)[y][x] = evalAt(x,y);
}
    
/* ------------------------------------------------------------------------ */

void POIFinder::evaluate()
{
    int border = 2.5f * (*std::max_element(scales.begin(), scales.end()));
    int w = src->getWidth(), h = src->getHeight();

    Array2D<float> globalProfile[steps];
    for(int i=0; i<steps; i++) {
        globalProfile[i] = Array2D<float>(w,h);
        globalProfile[i].fill(1.f);
    }

    {
        Array2D<float> currentProfile[steps];
        for(int i=0; i<steps; i++)
            currentProfile[i] = Array2D<float>(w,h);
        
        PrefixSums ps(*src);

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
    }

    Array2D<float> maxs(globalProfile[0]), mins(globalProfile[0]);
    
    for(int i=1; i<steps; i++)
        for(int y=border; y<h-border; y++)
            for(int x=border; x<w-border; x++) {
                maxs[y][x] = std::max(maxs[y][x], globalProfile[i][y][x]);
                mins[y][x] = std::min(mins[y][x], globalProfile[i][y][x]);
            }

    eval = Array2D<float>(w,h);
    eval.fill(0.f);
    for(int y=border; y<h-border; y++)
        for(int x=border; x<w-border; x++)
            eval[y][x] = powf(maxs[y][x] - mins[y][x], 1.0f/scales.size());
}

Image POIFinder::visualize()
{
    int w = eval.getWidth(), h = eval.getHeight();

    float max = 0;
    for(int y = 0; y < h; y++)
        for(int x = 0; x < w; x++)
            max = std::max(max, eval[y][x]);

    Image ret(w,h);
    for(int y=0; y<h; y++)
        for(int x=0; x<w; x++)
            ret[y][x] = round(eval[y][x]/max*255.f);

    return ret;
}

void POIFinder::getAll(void)
{
    int w = eval.getWidth(), h = eval.getHeight();

    all.clear();
    all.reserve(w*h);
    for(int y=0; y<h; y++)
        for(int x=0; x<w; x++)
            all.push_back(POI(x,y,eval[y][x]));
    std::sort(all.begin(), all.end());
}

void POIFinder::filter(void)
{
    int w = eval.getWidth(), h = eval.getHeight();
    Image tabu(w,h);
    tabu.fill(0);

    selected.clear();

    for(int i=0; i<(int)all.size() && (int)selected.size() < count; i++)
    {
        const POI &curr = all[i];
        int x = (int)curr.x, y = (int)curr.y;
        
        if(tabu[y][x]) continue;
        if(curr.val < threshold) break;
        
        selected.push_back(curr);
        
        float R = tabuScale / curr.val;
        int iR = (int)ceilf(R);

        for (int dy=-iR; dy<=iR; dy++)
            for (int dx=-iR; dx<=iR; dx++)
                if (dx*dx+dy*dy <= (int)(R*R) && tabu.inside(x+dx, y+dy))
                    tabu[y+dy][x+dx] = 1;
    }
}

void POIFinder::select(void)
{
    float downval = 1.f, upval = 3000.0f;
    
    int requestedCount = count;
    count = count * 4 / 3;

    while(upval-downval > 10.f)
    {
        tabuScale = .5f*(upval+downval);
        filter();

        if((int)selected.size() == requestedCount)
            break;

        if((int)selected.size() > requestedCount) 
            downval = tabuScale;
        else
            upval = tabuScale;
    }

    count = requestedCount;
}

void POIFinder::run(void)
{
    evaluate();
    getAll();

    if(tabuScale <= 0)
        select();
    else
        filter();
}

/* ------------------------------------------------------------------------ */

ProximityMap::ProximityMap(void) {
    width = height = detail = entries = widet = hedet = 0; 
    data = NULL;
}

ProximityMap::~ProximityMap(void) {
    free(data);
}

void ProximityMap::resize(int width, int height, int detail, int entries)
{
    this->width = width;
    this->height = height;
    this->detail = detail;
    this->entries = entries;
    widet = width * detail;
    hedet = height * detail;
    
    data = (poiid_t *)realloc(data, sizeof(poiid_t) * widet * hedet * entries);
    if(widet && hedet && entries && !data) throw std::bad_alloc();
}

ColorImage ProximityMap::visualize() const
{
    /* we do not perform map coloring here. just hope that given
     * so many colors, two adjacent areas won't be sharing one color too often */
    static const uint32_t colors[] = 
        { 0xffff0000, /* red */
          0xff00ff00, /* green */
          0xff0000ff, /* blue */
          0xffa0a000, /* yellow */
          0xffff40ff, /* violet */
          0xff20e0ff, /* cyan */
          0xffffc010, /* orange */
          0xffa00000, /* dark red */
          0xff008000, /* dark green */
          0xff0000a0, /* dark blue */
          0xffc3a823, /* ? */
          0xff99a103, /* ? */
          0xffc9ff2e, /* ? */
          0xff6349aa, /* ? */
          0xff9ff3e1, /* ? */
          0xff8efdca, /* ? */
          0xff6349aa, /* ? */
          0xff9efffe, /* ? */
          0xff834efa, /* ? */
          0xffabcdef, /* ? */
          0xff12e5fe, /* ? */
          0xff92cdff, /* ? */
          0
        };
    int n_colors = 0;
    while(colors[n_colors]) n_colors++;

    ColorImage ret(width, height);
    for(int y=0; y<height; y++)
        for(int x=0; x<width; x++)
            ret[y][x] = colors[at(x,y)[0] % n_colors];
    return ret;
}

void ProximityMap::pushEntry(const djk &o)
{
    poiid_t *k = &usedEntries[o.yi][o.xi];
    assert(*k < entries);
    _at(o.xi,o.yi)[(*k)++] = o.id;
}

/* check if o.id can visit field (o.xi, o,yi) */
bool ProximityMap::canVisit(const djk &o) const
{
    /* first check if the field exists */
    if(o.xi < 0 || o.yi < 0 || o.xi >= widet || o.yi >= hedet) 
        return false;

    /* then check if there's free space in the field */
    poiid_t k = usedEntries[o.yi][o.xi];
    if(k == entries) 
        return false;

    /* finally check if the field has been visited yet 
     * TODO: performance here can be propably improved */
    const poiid_t *entries = _at(o.xi,o.yi);
    for(int i=k-1; i>=0; i--)
        if(entries[i] == o.id)
            return false;
    
    return true;
}

void ProximityMap::build(const POIvec &pois)
{
    /* how many entries we still have to fill */
    int leftToDo = widet * hedet * entries;

    assert(pois.size() < 65536); /* because poiid_t is unsigned short */
    assert(entries <= (int)pois.size());

    /* this is temporary array that keeps track of how many entries were added
     * to each field. when the algoritm finishes, all fields have 'entries'
     * entries, so this array can be safely discarded */
    usedEntries = Array2D<poiid_t>(widet, hedet);
    usedEntries.fill(0);

    std::priority_queue<djk> Q;

    for(poiid_t i=0; i<(poiid_t)pois.size(); i++)
    {
        int xi = roundf(pois[i].x*detail),
            yi = roundf(pois[i].y*detail);
        Q.push(djk(0, xi, yi, i));
    }

    while(leftToDo)
    {
        assert(Q.size());
        djk o = Q.top();
        Q.pop();

        if(!canVisit(o))
            continue;
        pushEntry(o);
        leftToDo--;

        if(leftToDo % 10000 == 0) debug("proximity: left %d", leftToDo);

        for(int dx=-1; dx<=1; dx++)
            for(int dy=-1; dy<=1; dy++)
            {
                djk u;
                u.xi = o.xi + dx;
                u.yi = o.yi + dy;
                u.id = o.id;
                u.dist = (Point((float)u.xi / detail, (float)u.yi / detail) - pois[u.id]).dist();
                if(canVisit(u))
                    Q.push(u);
            }
    }

    /* free usedEntries' memory */
    usedEntries = Array2D<poiid_t>();
}

