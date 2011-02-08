#include <cstdio>
#include <cassert>
#include <cmath>
#include <vector>
#include <queue>
#include <set>
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

    float progress_step, *progress_done;

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

    /* warning: thread-unsafe! */
    if(progress_done)
        progress((*progress_done) += progress_step);
}
    
/* ------------------------------------------------------------------------ */

Array2D<float> evaluateImage(const Image &src, const std::vector<float> &scales, int steps)
{
    int border = 2.5f * (*std::max_element(scales.begin(), scales.end()));
    int w = src.getWidth(), h = src.getHeight();

    progress(0);

    Array2D<float> globalProfile[steps];
    for(int i=0; i<steps; i++) {
        globalProfile[i] = Array2D<float>(w,h);
        globalProfile[i].fill(1.f);
    }

    {
        Array2D<float> currentProfile[steps];
        for(int i=0; i<steps; i++)
            currentProfile[i] = Array2D<float>(w,h);
        
        PrefixSums ps(src);

        DifferenceJob jobs[steps];
        Completion c;

        float progress_step = 1.f/(steps * scales.size());
        float progress_done = 0.f;

        for(int i=0; i<steps; i++) {
            jobs[i].src = &ps;
            jobs[i].dst = &currentProfile[i];
            jobs[i].x1 = border;
            jobs[i].y1 = border;
            jobs[i].x2 = w-border;
            jobs[i].y2 = h-border;
            jobs[i].progress_step = progress_step;
            jobs[i].progress_done = &progress_done;
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

    Array2D<float> eval(w,h);
    eval.fill(0.f);
    for(int y=border; y<h-border; y++)
        for(int x=border; x<w-border; x++)
            eval[y][x] = powf(maxs[y][x] - mins[y][x], 1.0f/scales.size());
    return eval;
}

Image visualizeEvaluation(const Array2D<float> &eval)
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

POIvec extractPOIs(const Array2D<float> &eval, float threshold)
{
    int w = eval.getWidth(), h = eval.getHeight();

    POIvec all;
    for(int y=0; y<h; y++)
        for(int x=0; x<w; x++)
            if(eval[y][x] >= threshold)
                all.push_back(POI(x,y,eval[y][x]));
    std::sort(all.begin(), all.end());
    return all;
}

static inline bool tmpOper(const std::pair<POI,int> &p1, const std::pair<POI,int> &p2) {
    return POIlessX(p1.first, p2.first);
}
POIvec filterPOIs(const POIvec &all, int count, float tabuScale, const Matrix &M)
{
    float minx = 1000000000, maxx = -1000000000,
          miny = 1000000000, maxy = -1000000000;
    for(int i=0; i<(int)all.size(); i++) {
        POI p = M * all[i];
        minx = std::min(minx, p.x);
        maxx = std::max(maxx, p.x);
        miny = std::min(miny, p.y);
        maxy = std::max(maxy, p.y);
    }
    
    minx -= 10; miny -= 10;
    maxx += 10; maxy += 10;

    int w = ceilf(maxx - minx), h = ceilf(maxy - miny);
    Image tabu(w,h);
    tabu.fill(0);

    POIvec selected;

    for(int i=0; i<(int)all.size() && (int)selected.size() < count; i++)
    {
        POI p = M * all[i];
        int x = roundf(p.x - minx), y = roundf(p.y - miny);
        
        if(tabu[y][x]) continue;
        
        selected.push_back(p);
        
        float R = tabuScale / p.val;
        int iR = (int)ceilf(R);

        for (int dy=-iR; dy<=iR; dy++)
            for (int dx=-iR; dx<=iR; dx++)
                if (dx*dx+dy*dy <= (int)(R*R) && tabu.inside(x+dx, y+dy))
                    tabu[y+dy][x+dx] = 1;
    }

    return selected;
}

POIvec filterPOIs(const POIvec &all, int count, float *foundTabu /*= NULL*/)
{
    float downval = 1.f, upval = 7000.0f, midval;

    Matrix id;
    POIvec selected;

    while(upval-downval > 2.f)
    {
        midval = .5f*(upval+downval);
        selected = filterPOIs(all, all.size() /* *4/3 ???*/, midval, id);

        if((int)selected.size() == count)
            break;

        if((int)selected.size() > count) 
            downval = midval;
        else
            upval = midval;
    }
    
    if (foundTabu)
        *foundTabu = midval;
    
    return selected;
}

/* ------------------------------------------------------------------------ */

class ProximityMapVisualizer
{
    typedef ProximityMap::poiid_t poiid_t;

    const ProximityMap *prox;
    int ent;
    int npois;
    
    std::vector<int> colors;
    std::vector<std::vector<poiid_t> > edges;

    void makeGraph();
    void dfs(int v);
    void findColoring(void);
    ColorImage createImage();

public:
    ColorImage build(const ProximityMap &prox, int ent);
};

void ProximityMapVisualizer::makeGraph()
{
    int width = prox->getWidth(),
        height = prox->getHeight();

    std::set<std::pair<poiid_t, poiid_t> > S;

    npois = 0;

    for(int y=1; y<height; y++)
        for(int x=1; x<width; x++) {
            poiid_t here = prox->at(x,y)[ent],
                    a    = prox->at(x-1,y)[ent],
                    b    = prox->at(x,y-1)[ent],
                    c    = prox->at(x-1,y-1)[ent];
            if(here != a)
                S.insert(std::make_pair(std::min(a, here), std::max(a, here)));
            if(here != b)
                S.insert(std::make_pair(std::min(b, here), std::max(b, here)));
            if(here != c)
                S.insert(std::make_pair(std::min(c, here), std::max(c, here)));

            npois = std::max(npois, (int)here);
            npois = std::max(npois, (int)a);
            npois = std::max(npois, (int)b);
            npois = std::max(npois, (int)c);
        }
    npois++;

    edges.clear();
    edges.resize(npois);
    for(std::set<std::pair<poiid_t, poiid_t> >::iterator it = S.begin(); it != S.end(); it++) {
        edges[it->first].push_back(it->second);
        edges[it->second].push_back(it->first);
    }

    colors.resize(npois);
    for(int i=0; i<npois; i++)
        colors[i] = -1;
}

void ProximityMapVisualizer::dfs(int v)
{
    bool used[12] = { };

    for(int i=0; i<(int)edges[v].size(); i++)
        if(colors[edges[v][i]] != -1) 
            used[colors[edges[v][i]]] = true;

    int mycolor = -1;
    for(int i=0; i<12; i++)
        if(!used[i]) {
            mycolor = i;
            break;
        }
    if(mycolor == -1) {
        warn("unable to dumb-12-color graph");
        mycolor = 0;
    }

    colors[v] = mycolor;
    
    for(int i=0; i<(int)edges[v].size(); i++)
        if(colors[edges[v][i]] == -1) 
            dfs(edges[v][i]);
}

void ProximityMapVisualizer::findColoring(void)
{
    for(int i=0; i<npois; i++)
        if(colors[i] == -1)
            dfs(i);
}

ColorImage ProximityMapVisualizer::createImage()
{
    static const uint32_t rgba[12] = /* 12 colors */
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
          0xff99a103, /* ? */ };

    int width = prox->getWidth(),
        height = prox->getHeight();

    ColorImage ret(width, height);
    for(int y=0; y<height; y++)
        for(int x=0; x<width; x++)
            ret[y][x] = rgba[colors[prox->at(x,y)[ent]]];
    return ret;
}

ColorImage ProximityMapVisualizer::build(const ProximityMap &prox, int ent)
{
    this->prox = &prox;
    this->ent = ent;

    makeGraph();
    findColoring();
    return createImage();
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
    ProximityMapVisualizer vis;
    return vis.build(*this, 0);
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
    int allToDo = widet * hedet * entries,
        leftToDo = allToDo;

    int npois = (int)pois.size();
    assert(npois < 65536); /* because poiid_t is unsigned short */
    assert(entries <= npois);

    progress(0);

    /* this is temporary array that keeps track of how many entries were added
     * to each field. when the algoritm finishes, all fields have 'entries'
     * entries, so this array can be safely discarded */
    usedEntries = Array2D<poiid_t>(widet, hedet);
    usedEntries.fill(0);

    std::priority_queue<djk> Q;

    for(poiid_t i=0; i<npois; i++)
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

        if(leftToDo % 20000 == 0) 
            progress((float)(allToDo - leftToDo) / (float)allToDo);

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

