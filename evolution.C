#include <cstdlib>
#include <cstdio>
#include <cmath>
#include <cassert>
#include <ctime>
#include <ctype.h>
#include <locale.h>
#include <sys/stat.h>
#include <vector>
#include <string>
#include <bitset>
#include <algorithm>
#include <stdexcept>

#include "util.h"
#include "poi.h"
#include "gui.h"
#include "image.h"
#include "config.h"

#define info(fmt, ...)  printf("   " fmt "\n", ## __VA_ARGS__)
#define okay(fmt, ...)  printf(" + " fmt "\n", ## __VA_ARGS__)
#define warn(fmt, ...)  printf("-- " fmt "\n", ## __VA_ARGS__)
#define fail(fmt, ...)  printf("!! " fmt "\n", ## __VA_ARGS__)
#define debug(fmt, ...) printf(".. " fmt "\n", ## __VA_ARGS__)

#define TRYCHOICE 0

#define INF 1e8f

/* ------------------------------------------------------------------------ */

static void parsePOIScales(const char *value);
static void parseSurvivalEq(const char *value);

/* this is the size of bitset in Agent */
#define MAX_POIS 256
typedef std::bitset<MAX_POIS> mask_t;

static int cfgThreads;
static int cfgPOISteps;
static std::vector<float> cfgPOIScales;
static float cfgPOIThreshold;
static float cfgPOITabuParam;
static int cfgPOICount;
static int cfgPopulationSize;
static std::vector<float> cfgSurvivalEq;
static int cfgStopCondParam, cfgMaxGenerations;
static float cfgTranslateInit, cfgRotateInit, cfgScaleInit;
static float cfgTranslateProp,  cfgRotateProp,  cfgScaleProp, cfgFlipProp;
static float cfgTranslateDev,   cfgRotateDev,   cfgScaleDev;
static float cfgTranslateDev2,  cfgRotateDev2,  cfgScaleDev2;
static float cfgTranslateMean2, cfgRotateMean2, cfgScaleMean2;
static float cfgOriginDev;
static float cfgDEMatingProp, cfgDEMatingCoeff, cfgDEMatingDev;
static float cfgMaxScaleRatio;
static float cfgMinTakenPoints = 0.15;
static float cfgChoiceProp = 0.2;
static float cfgChoiceDev = 3;


static struct config_var cfgvars[] = {
    { "threads",        config_var::INT,       &cfgThreads },
    /* poi detection */
    { "poiSteps",       config_var::INT,       &cfgPOISteps },
    { "poiScales",      config_var::CALLBACK,  (void *)&parsePOIScales },
    { "poiCount",       config_var::INT,       &cfgPOICount },
    { "poiThreshold",   config_var::FLOAT,     &cfgPOIThreshold },
    { "poiTabuParam",   config_var::FLOAT,     &cfgPOITabuParam },
    /* evolution */
    { "populationSize", config_var::INT,       &cfgPopulationSize },
    { "survivalEq",     config_var::CALLBACK,  (void *)&parseSurvivalEq },
    { "stopCondParam",  config_var::INT,       &cfgStopCondParam },
    { "maxGenerations", config_var::INT,       &cfgMaxGenerations },
    /* target function */
    { "maxScaleRatio",  config_var::FLOAT,     &cfgMaxScaleRatio },
    /* initial population generation parameters */
    { "translateInit",  config_var::FLOAT,     &cfgTranslateInit },
    { "rotateInit",     config_var::FLOAT,     &cfgRotateInit },
    { "scaleInit",      config_var::FLOAT,     &cfgScaleInit },
    /* mutation propabilities and standard deviations (gaussian distribution) */
    { "translateProp",  config_var::FLOAT,     &cfgTranslateProp },
    { "rotateProp",     config_var::FLOAT,     &cfgRotateProp },
    { "scaleProp",      config_var::FLOAT,     &cfgScaleProp },
    { "flipProp",       config_var::FLOAT,     &cfgFlipProp },
    { "translateDev",   config_var::FLOAT,     &cfgTranslateDev },
    { "rotateDev",      config_var::FLOAT,     &cfgRotateDev },
    { "scaleDev",       config_var::FLOAT,     &cfgScaleDev },
    { "translateMean2", config_var::FLOAT,     &cfgTranslateMean2 },
    { "rotateMean2",    config_var::FLOAT,     &cfgRotateMean2 },
    { "scaleMean2",     config_var::FLOAT,     &cfgScaleMean2 },
    { "translateDev2",  config_var::FLOAT,     &cfgTranslateDev2 },
    { "rotateDev2",     config_var::FLOAT,     &cfgRotateDev2 },
    { "scaleDev2",      config_var::FLOAT,     &cfgScaleDev2 },
    /* standard deviation of rotation/scaling origin from the median point */
    { "originDev",      config_var::FLOAT,     &cfgOriginDev },
    /* differential evolution mating settings */
    { "deMatingProp",   config_var::FLOAT,     &cfgDEMatingProp },
    { "deMatingCoeff",  config_var::FLOAT,     &cfgDEMatingCoeff },
    { "deMatingDev",    config_var::FLOAT,     &cfgDEMatingDev },
    /* end-of-table terminator, must be here! */
    { NULL,             config_var::NONE,      NULL }
};

static std::vector<float> parseFloatVector(const char *value)
{
    std::vector<float> ret;
    while(*value)
    {
        char *end;
        float v = strtof(value, &end);
        if(end == value)
            throw std::runtime_error("failed to parse float");
        
        ret.push_back(v);

        while(isspace(*end)) end++;
        if(*end == ',') end++;
        while(isspace(*end)) end++;

        value = end;
    }
    return ret;
}

static void parsePOIScales(const char *value) {
    cfgPOIScales = parseFloatVector(value);
}
static void parseSurvivalEq(const char *value) {
    cfgSurvivalEq = parseFloatVector(value);
}

/* ------------------------------------------------------------------------ */

class Random
{
public:
    static inline bool coin() {
        return rand()&1;
    }
    static inline float randomSign() {
        return coin() ? -1.0 : 1.0;
    }
    static inline bool maybe(float prop) {
        return rand() <= prop*RAND_MAX;
    }
    static inline float positive(float max) {
        return max*((float)rand() / RAND_MAX);
    }
    static inline float real(float max) {
        return coin() ? positive(max) : -positive(max);
    }
    static float gaussian(float mean, float deviation) {
        /* Box-Muller transform ftw ;) */
        float p = positive(1), q = positive(1);
        float g = sqrtf(-2.0f * logf(p)) * cosf(2.0f*M_PI*q);
        return g*deviation + mean;
    }
    static float trigauss(float m1, float d1, float m2, float d2) {
        return (gaussian(m1,d1) + gaussian(m1+m2,d2) + gaussian(m1-m2,d2))/3.f;
    }
};

/* ------------------------------------------------------------------------ */

class Data
{
    inline void findOrigin();
    inline void findPOIs();

    static inline char *makeCacheFilename(const char *filename);
    inline uint32_t checksum() const;
    inline void readCached(const char *filename);
    inline void writeCache(const char *filename) const;

    struct cacheHdr {
        enum { MAGIC = 0xfc83dea1 };
        uint32_t magic;
        uint32_t checksum;
        uint32_t poiCount;
        uint32_t originX, originY;
    };

    inline Data() { }
    void doBuild(const char *filename);

public:
    Image raw;
    std::vector<POI> pois;
    int originX, originY; /* median of POIs */
    CairoImage caimg; /* image for gui */

    static inline Data build(const char *filename) {
        Data ret;
        ret.doBuild(filename);
        return ret;
    }
};

void Data::doBuild(const char *filename)
{
    raw = Image::read(filename);

    try {
        readCached(filename);
    } catch(std::exception &e) {
        warn("failed to read cache: %s", e.what());
        findPOIs();
        findOrigin(); 
        writeCache(filename);
    }
    std::sort(pois.begin(), pois.end(), compX);
    caimg = gui_upload(raw);
}

void Data::findPOIs()
{
    Array2D<float> eval = ::evaluateImage(raw, cfgPOISteps, cfgPOIScales);
    pois = ::findPOIs(eval, cfgPOIThreshold, cfgPOICount, cfgPOITabuParam);
}

void Data::findOrigin()
{
    std::vector<int> v;
    
    for(int i=0; i<(int)pois.size(); i++)
        v.push_back(pois[i].x);
    std::sort(v.begin(), v.end());
    originX = v[pois.size()/2];

    v.clear();
    for(int i=0; i<(int)pois.size(); i++)
        v.push_back(pois[i].y);
    std::sort(v.begin(), v.end());
    originY = v[pois.size()/2];
}

static inline uint32_t float2u32(float v) {
    union { float x; uint32_t y; } aa;
    aa.x = v;
    return aa.y;
}
uint32_t Data::checksum() const
{
    uint32_t ret = raw.checksum();
    ret ^= cfgPOISteps ^ cfgPOICount;
    ret ^= float2u32(cfgPOIThreshold);
    for(int i=0; i<(int)cfgPOIScales.size(); i++)
        ret ^= float2u32(cfgPOIScales[i]);
    return ret;
}

char *Data::makeCacheFilename(const char *filename)
{
    int len = strlen(filename);
    assert(len >= 4);

    char *ret = strdup(filename);
    ret[len-4] = '.'; ret[len-3] = 'd'; ret[len-2] = 'a'; ret[len-1] = 't';
    return ret;
}
void Data::readCached(const char *filename)
{
    char *cacheFilename = makeCacheFilename(filename);
    FILE *f = fopen(cacheFilename, "rb");
    free(cacheFilename);

    if(!f) 
        throw std::runtime_error("failed to open cache file");

    struct cacheHdr hdr;
    if(fread(&hdr, sizeof(hdr),1, f) != 1) {
        fclose(f);
        throw std::runtime_error("truncated cache file header");
    }

    if(hdr.magic != cacheHdr::MAGIC) {
        fclose(f);
        throw std::runtime_error("invalid cache file magic number");
    }

    if(hdr.checksum != checksum()) {
        fclose(f);
        throw std::runtime_error("cache file checksum mismatch");
    }

    pois.resize(hdr.poiCount);
    if(fread(&pois[0], sizeof(POI),hdr.poiCount, f) != hdr.poiCount) {
        pois.clear(); fclose(f);
        throw std::runtime_error("failed to read POIs");
    }

    originX = hdr.originX;
    originY = hdr.originY;

    fclose(f);
}

void Data::writeCache(const char *filename) const
{
    char *cacheFilename = makeCacheFilename(filename);
    FILE *f = fopen(cacheFilename, "wb");
    free(cacheFilename);

    if(!f) 
        throw std::runtime_error("failed to write cache file");

    cacheHdr hdr;
    hdr.magic = cacheHdr::MAGIC;
    hdr.checksum = checksum();
    hdr.poiCount = pois.size();
    hdr.originX = originX;
    hdr.originY = originY;

    if(fwrite(&hdr, sizeof(hdr),1, f) != 1 ||
       fwrite(&pois[0], sizeof(POI),pois.size(), f) != pois.size()) {
        fclose(f);
        throw std::runtime_error("failed to write cache file");
    }

    fclose(f);
}

/* ------------------------------------------------------------------------ */

class ImageDisplaySlot : public DisplaySlot
{
public:
    const Data *data;
    rgba color;

    inline ImageDisplaySlot(const char *name, rgba color)
        : DisplaySlot(name), color(color) { }
    virtual ~ImageDisplaySlot() { }

    virtual void draw()
    {
        if(!data)
            return;

        resize(data->raw.getWidth(), data->raw.getHeight());
        drawImage(data->caimg);

        std::vector<Point> ps(data->pois.size());
        for(int i=0; i<(int)data->pois.size(); i++)
            ps[i] = data->pois[i];
        drawDots(ps,6, color);
    }
};

class FitDisplaySlot : public DisplaySlot
{
public:
    const Data *known, *alien;
    rgba knownActiveColor, knownInactiveColor, alienColor, silhouetteColor;
    mask_t knownMask;
    std::vector<Matrix> ms;

    inline FitDisplaySlot(const char *name) : DisplaySlot(name) 
    {
        known = alien = NULL;
        knownActiveColor = rgba(0.1, 1, 0.1, 0.5);
        knownInactiveColor = rgba(.7, .7, .7, 0.7);
        alienColor = rgba(1,0.3,0.1,0.5);
        silhouetteColor = rgba(.2,.5,1,.3);
    }

    virtual ~FitDisplaySlot() { }

    virtual void draw()
    {
        if(!known || !alien || ms.size() == 0)
            return;

/*      Timer tim;
        tim.start(); */

        resize(alien->raw.getWidth(), alien->raw.getHeight());
        drawImage(alien->caimg);
        drawDifference(known->caimg, ms[0]);

        drawSilhouettes(ms, known->raw.getWidth(), known->raw.getHeight(), silhouetteColor);
        
        std::vector<Point> ps(alien->pois.begin(), alien->pois.end());
        drawDots(ps,6, alienColor);

        ps.clear();
        for(int i=0; i<(int)known->pois.size(); i++)
            if(knownMask[i])
                ps.push_back(ms[0] * known->pois[i]);
        drawDots(ps,6, knownActiveColor);
        
        ps.clear();
        for(int i=0; i<(int)known->pois.size(); i++)
            if(!knownMask[i])
                ps.push_back(ms[0] * known->pois[i]);
        drawDots(ps,6, knownInactiveColor);
        
/*      debug("drawing took %.6f ms", tim.end()*1000.f); */
    }
};

/* those are created statically, however they will only initialize
 * themselves upon first action, which must occur after gtk_gui_init. */

static ImageDisplaySlot knownDS("known image", rgba(0.1,1,0.1,0.5)),
                        alienDS("alien image", rgba(1,0.3,0.1,0.5));
static FitDisplaySlot   bestDS("best fit");

/* ------------------------------------------------------------------------ */

class Agent
{
public:
    Matrix M; /* the transformation matrix */
    
    int validCnt;
    
    int mx, Mx, my, My; /* upper and lower bounds for points that are taken into consideration */
    mask_t mask; /* it might be useful */
    
    float target; /* target function */
    float fitness; /* population-wide fitness factor */

    /* agents ordering */
    inline bool operator<(const Agent &A) const {
        return fitness > A.fitness;
    }
    
    /* transformation matrix decomposition */
    inline float dx() const { return M[0][2]; }
    inline float dy() const { return M[1][2]; }
    inline float sx() const { return sqrtf(M[0][0]*M[0][0]+M[0][1]*M[0][1]); }
    inline float sy() const { return sqrtf(M[1][0]*M[1][0]+M[1][1]*M[1][1]); }
    inline float alfa() const { 
        float s = sqrtf(M[0][0]*M[0][0]+M[0][1]*M[0][1]); 
        return atan2f(M[0][1]/s, M[0][0]/s);
    }

    inline void translate(float dx, float dy);
    inline void rotate(float angle, float ox, float oy);
    inline void scale(float sx, float sy, float ox, float oy);   
    
    inline bool valid(const Point& p) const { return (p.x >= mx && p.y >= my && p.x <= Mx && p.y <= My); }
};

/* the three basic mutations.
 *
 * the (ox,oy) is transformation origin specified in image local
 * coordinates. transformation will take place relative to the
 * current location of this given point (that is, at M*(ox,oy)).
 *
 * matricies are PRE-multiplied, because transforms work like
 *    y = ... M3 * M2 * M1 * x
 * and we want newly applied transformation to come last. */
void Agent::translate(float dx, float dy) {
    M = Matrix::translation(dx,dy)
      * M;
}
void Agent::rotate(float angle, float ox, float oy) {
    Point origin = M * POI(ox,oy,0);
    M = Matrix::translation(origin.x,origin.y)
      * Matrix::rotation(angle)
      * Matrix::translation(-origin.x,-origin.y)
      * M;
}
void Agent::scale(float sx, float sy, float ox, float oy) {
    Point origin = M * POI(ox,oy,0);
    M = Matrix::translation(origin.x,origin.y)
      * Matrix::scaling(sx, sy)
      * Matrix::translation(-origin.x,-origin.y)
      * M;
}

/* ------------------------------------------------------------------------ */

class Population
{
    /* we're trying to match the Known image to the Alien image */
    const Data *known, *alien;

    /* our lovely little things */
    std::vector<Agent> pop;

    /* history record of best scores, one entry per generation */
    std::vector<float> bestScores;

    /* current generation number */
    int generationNumber;

    /* population evaluation (multi-threaded) */
    class EvaluationJob : public AsyncJob
    {
        void runOne(Agent *agent);

    public:
        Population *uplink;
        int start, end;
        virtual ~EvaluationJob();
        virtual void run();

    };
    inline void evaluate();

    /* roulette selection */
    int roulette(int n);

    /* mutations and matings */
    inline void makeRandom(Agent *a);
    inline void mutation(Agent *a);
    inline void deMating(Agent *a, const Agent *p, const Agent *q, const Agent *r);

    /* one needs to know when to stop! */
    inline bool terminationCondition() const;

    /* how many specimen will advance to the next generation */
    float getSurvivalRate() const;

public:
    static float distance(std::vector<Point> &base, const std::vector<Point> &query, uint8_t K, int* foo);
    Population(const Data *known, const Data *alien);
    Agent evolve();
};

/* --- population evaluation */
Population::EvaluationJob::~EvaluationJob() {
    /* empty */
}

void Population::EvaluationJob::run()
{
    for(int i=start; i<end; i++)
        runOne(&uplink->pop[i]);
}

/* base -- aliens, query -- known */
float Population::distance(std::vector<Point> &base, const std::vector<Point> &query, uint8_t K, int *foo = NULL)
{
    /*TEST*/int TESTloop = 0;
    
    uint8_t cnts[base.size()]; 
    memset(cnts, 0, sizeof(cnts));
    
    float sum = 0;
    for(int i=0; i<(int)query.size(); i++)
    {
        Point p = query[i];
        
        int bestidx = -1; float bestdist = 1e+10f;
        int ind0 = upper_bound(base.begin(), base.end(), p) - base.begin();
        /* najpierw idziemy po rosnących x */
        for (int j = ind0; j<(int)base.size() && abs(base[j].x-p.x) <= bestdist; j++) 
        {
            /*TEST*/TESTloop ++;
            if (cnts[j] >= K) continue;
            float dist = (p - base[j]).dist();
            if(dist >= bestdist) continue;
            bestidx = j;
            bestdist = dist;
        }
        /* potem po malejących x */
        for (int j = ind0-1; j>=0 && abs(p.x-base[j].x) <= bestdist; j--) 
        {
            /*TEST*/TESTloop ++;
            if (cnts[j] >= K) continue;
            float dist = (p - base[j]).dist();
            if(dist >= bestdist) continue;
            bestidx = j;
            bestdist = dist;
        }

        if(bestidx != -1) {
            cnts[bestidx]++;
            sum += bestdist;
        } else {
            warn("cannot match poi");
            sum += INF;
        }
    }
    /*TEST*/if (foo != NULL) *foo += TESTloop;
    
    /* compute average number of queries matched to one base point */
    int nzeroCnt=0, nzeroSum=0;
    for (int i=0; i<(int)base.size(); i++)
        if (cnts[i]) {
            nzeroCnt ++;
            nzeroSum += cnts[i];
        }
    float avgMatch = (float)nzeroSum/nzeroCnt;
    sum *= std::min(1000.f, expf(avgMatch-1));
    
    return sum;
}

inline float vectorSpan(const std::vector<Point> &v)
{
    float minx=INF,miny=INF,maxx=-INF,maxy=-INF;
    for (int i=0; i<(int)v.size(); i++) {
        minx = std::min(minx, v[i].x);
        maxx = std::max(maxx, v[i].x);
        miny = std::min(miny, v[i].y);
        maxy = std::max(maxy, v[i].y);
    }
    return sqrtf((float)(maxx-minx+1.f)*(maxx-minx+1.f) + (float)(maxy-miny+1.f)*(maxy-miny+1.f));
}

inline float imageSpan(const Image &im, const Matrix &M)
{
    return (
        M*Point(im.getWidth(), im.getHeight())
      - M*Point(.0f, 0.f)
    ).dist();
}
inline float imageSpan(const Image &im) { return imageSpan(im, Matrix()); }

void Population::EvaluationJob::runOne(Agent *agent)
{
    const Data *alien = uplink->alien, *known = uplink->known;
    const uint8_t K = 100;
    
    /* for each POI from known image that is allowed by agent's RECTANGLE
     * we look for closest POI from alien image. moreover, each alien
     * POI can be used at most K times (this is to prevent matching
     * all known POIs with one or two alien POIs). */
    
    Matrix revAgent = agent->M.inverse();
    
    std::vector<Point> aliens, knowns, validaliens;
    for (int i=0; i<(int)alien->pois.size(); i++) 
    {
        if (agent->valid(revAgent*alien->pois[i]))
            validaliens.push_back(alien->pois[i]);
        aliens.push_back(alien->pois[i]);
    }
    for (int i=0; i<(int)known->pois.size(); i++) {
        Point p = known->pois[i];
        if (agent->valid(p)) {
            knowns.push_back(agent->M*p);
            agent->mask[i] = true;
        }
        else
            agent->mask[i] = false;
    }
    agent->validCnt = knowns.size();
    
    // std::sort(knowns.begin(), knowns.end());
    // std::sort(aliens.begin(), aliens.end());
    
    float takenRatio = (float)agent->validCnt/known->pois.size(),
          dist = 0, dist2 = 0;

    /*TEST*/static int which=0; int oper=0;
    dist = distance(aliens, knowns, K, &oper) 
        / (aliens.size() + knowns.size());
        
    if (false && validaliens.size())
            dist2 = distance(knowns, validaliens, K, &oper)
            / validaliens.size() ;
    
    agent->target = -(dist+dist2) / pow(takenRatio, 1.);
    if (takenRatio < cfgMinTakenPoints)
        agent->target += INF;
        
    /*TEST*/if ((which++)%5000 == 0) 
        debug("%d vs %d: dist=%.3f+%.3f, valid=%d\n", aliens.size()*agent->validCnt, oper, dist, dist2, agent->validCnt);
}
void Population::evaluate()
{
    const int evalBatch = 50;
    int nJobs = (pop.size() + evalBatch - 1) / evalBatch;
    EvaluationJob jobs[nJobs];
    Completion c;
    
    for(int i=0; i<nJobs; i++) {
        jobs[i].completion = &c;
        jobs[i].uplink = this;
        jobs[i].start = evalBatch*i;
        jobs[i].end = std::min((int)pop.size(), evalBatch*(i+1));
    }

    for(int i=0; i<nJobs; i++)
        aq->queue(&jobs[i]);
    c.wait();

    float minTarget = 1e+30;
    for(int i=0; i<(int)pop.size(); i++)
        minTarget = std::min(minTarget, pop[i].target);

    float denominator = 0;
    for(int i=0; i<(int)pop.size(); i++)
        denominator += pop[i].target - minTarget;
    assert(denominator > 1e-5);
    denominator = 1.f/denominator;

    for(int i=0; i<(int)pop.size(); i++)
        pop[i].fitness = (pop[i].target - minTarget) * denominator;

    std::sort(pop.begin(), pop.end());
}

/* --- roulette selection */
int Population::roulette(int n)
{
    float x = 0;
    for(int i=0; i<n; i++)
        x += pop[i].fitness;

    float y = Random::positive(x);
    for(int i=0; i<n; i++)
        if(y <= pop[i].fitness)
            return i;
        else
            y -= pop[i].fitness;

    warn("roulette selection failed, residual %e", y);
    return 0;
}

/* --- mutations */
void Population::makeRandom(Agent *a)
{
    a->mask.set(); /* not too random for now */
    
    int W = known->raw.getWidth(), 
        H = known->raw.getHeight();
    
#if TRYCHOICE == 1
    float dx = Random::positive(W/3)+2.0f*W/3, x1 = Random::positive(W-dx),
          dy = Random::positive(H/3)+2.0f*H/3, y1 = Random::positive(H-dy);
          
    a->mx = x1;
    a->Mx = x1+dx;
    a->my = y1;
    a->My = y1+dy;
    
    assert(a->valid(Point(a->mx,a->my)));
    assert(a->valid(Point(a->Mx,a->My)));
    assert(a->valid(Point(a->Mx - a->my, a->My - a->my)));
#else
    a->mx = a->my = 0;
    a->Mx = W;
    a->My = H;
#endif

    a->M = Matrix();
    a->translate(
            Random::real(cfgTranslateInit) * known->raw.getWidth(),
            Random::real(cfgTranslateInit) * known->raw.getHeight());
    a->rotate(Random::real(cfgRotateInit),
            known->originX, known->originY);
    a->scale(
        /*(1.f/Random::positive(cfgScaleInit))
            * Random::randomSign(),
        (1.f/Random::positive(cfgScaleInit))
            * Random::randomSign(),*/
        Random::real(cfgScaleInit),
        Random::real(cfgScaleInit),
        known->originX, known->originY);
}
void Population::mutation(Agent *a)
{
    float w = known->raw.getWidth(),
          h = known->raw.getHeight();

    if(Random::maybe(cfgTranslateProp))
        a->translate(Random::trigauss(0, cfgTranslateDev, cfgTranslateMean2, cfgTranslateDev2) * w,
                     Random::trigauss(0, cfgTranslateDev, cfgTranslateMean2, cfgTranslateDev2) * h);

    if(Random::maybe(cfgRotateProp))
        a->rotate(Random::trigauss(0, cfgRotateDev, cfgRotateMean2, cfgRotateDev2),
                  Random::gaussian(known->originX, cfgOriginDev * w),
                  Random::gaussian(known->originY, cfgOriginDev * h));

    if(Random::maybe(cfgScaleProp))
        a->scale(Random::trigauss(1, cfgScaleDev, cfgScaleMean2, cfgScaleDev2),
                 Random::trigauss(1, cfgScaleDev, cfgScaleMean2, cfgScaleDev2),
                 Random::gaussian(known->originX, cfgOriginDev * w),
                 Random::gaussian(known->originY, cfgOriginDev * h));

    if(Random::maybe(cfgFlipProp))
    {
        float rangle = Random::real(2.f*M_PI),
              rx = Random::gaussian(known->originX, cfgOriginDev*w),
              ry = Random::gaussian(known->originY, cfgOriginDev*h);
        
        a->rotate(rangle, rx, ry);
        a->scale(-1, 1, rx, ry);
        a->rotate(-rangle, rx, ry);
    }
#if TRYCHOICE == 1
    if (Random::maybe(cfgChoiceProp)) a->mx += Random::gaussian(0, cfgChoiceDev);
    if (Random::maybe(cfgChoiceProp)) a->Mx += Random::gaussian(0, cfgChoiceDev);
    if (Random::maybe(cfgChoiceProp)) a->my += Random::gaussian(0, cfgChoiceDev);
    if (Random::maybe(cfgChoiceProp)) a->My += Random::gaussian(0, cfgChoiceDev);
#endif
}

/* --- differential evolution mating */
void Population::deMating(Agent *a, const Agent *p, const Agent *q, const Agent *r)
{
    float mfactor = fabs(Random::gaussian(cfgDEMatingCoeff, cfgDEMatingDev));
    if(q->fitness < r->fitness) std::swap(q,r);
    a->M = p->M + (q->M - r->M) * mfactor;
    /* now rectangle demating */
#if TRYCHOICE == 1
    float cfactor = fabs(Random::gaussian(cfgDEMatingCoeff, cfgDEMatingDev));
    a->mx = p->mx + (q->mx - r->mx) * cfactor;
    a->Mx = p->Mx + (q->Mx - r->Mx) * cfactor;
    a->my = p->my + (q->my - r->my) * cfactor;
    a->My = p->My + (q->My - r->My) * cfactor;
#endif
}

/* this tells us when to stop */
bool Population::terminationCondition() const
{
    if(generationNumber > cfgMaxGenerations)
        return true;

    return false; /* na razie wylaczony */
    const int K = cfgStopCondParam;
    if ((int)bestScores.size() < 2*K)
        return false;

    /* jesli przez ostatnie K pokolen nie stalo sie nic ciekawszego niz podczas poprzednich K */
    float max1 = *std::max_element(bestScores.end()-K,   bestScores.end()),
          max2 = *std::max_element(bestScores.end()-2*K, bestScores.end()-K);

    return max2 >= max1;
}

/* how many specimen will make it to the next round */
float Population::getSurvivalRate() const
{
    float v = 0, x = (float) (generationNumber-1) / (cfgMaxGenerations-1);
    for(int i=0; i<(int)cfgSurvivalEq.size(); i++)
        v = x*v + cfgSurvivalEq[i];
    assert(v >= 0 && v <= 1);
    return v;
}
    
/* --- the so called main loop */
Agent Population::evolve()
{
    pop.resize(cfgPopulationSize);
    for(int i=0; i<(int)pop.size(); i++)
        makeRandom(&pop[i]);

    knownDS.lock(); knownDS.data = known; knownDS.unlock();
    alienDS.lock(); alienDS.data = alien; alienDS.unlock();
    bestDS.lock(); bestDS.known = known; bestDS.alien = alien; bestDS.unlock();

    Agent bestEver;
    bestEver.target = -1000000.0f;
    
    std::vector<std::vector<float> > logVector;
    static const int LOGperGEN = 10;

    for(generationNumber = 1; ; generationNumber++)
    {
        evaluate();

        float survivalRate = getSurvivalRate();

        gui_status("gen: %d, survival: %d%%, best fit: target %.2f fitness %f  |  "
                   "d=(%.0f,%.0f), s=(%.2f,%.2f), a=%.3f  |   "
                   "valid=%d,  mx=%d, Mx=%d, my=%d, My=%d",
                   generationNumber, (int)(survivalRate*100.f),
                   pop[0].target, pop[0].fitness,
                   pop[0].dx(),pop[0].dy(),pop[0].sx(),pop[0].sy(),pop[0].alfa()/M_PI*180.0,
                   pop[0].validCnt,
                   pop[0].mx, pop[0].Mx, pop[0].my, pop[0].My);

        bestDS.lock();
        bestDS.ms.clear();
        for(int i=0; i<(int)pop.size(); i+=std::max(1, (int)pop.size()/30))
            bestDS.ms.push_back(pop[i].M);
        bestDS.knownMask = pop[0].mask;
        bestDS.unlock();

        bestScores.push_back(pop[0].target);
        if (bestEver.target < pop[0].target)
            bestEver = pop[0];
        
        {
            logVector.push_back(std::vector<float>(10));
            logVector.back()[0] = pop[0].target;
            for (int i=1; i<LOGperGEN; i++)
                logVector.back()[i] = pop[rand()%pop.size()/2].target;
        }
        
        int survivors = survivalRate * pop.size();
        for(int i=survivors; i<(int)pop.size(); i++)
            if(Random::maybe(cfgDEMatingProp))
            {
                int pi,qi,ri;
                pi = roulette(survivors);
                do qi = roulette(survivors); while (qi == pi);
                do ri = roulette(survivors); while (ri == qi || ri == pi);
                deMating(&pop[i], &pop[pi], &pop[qi], &pop[ri]);
            } else
                pop[i] = pop[roulette(survivors)];

        for(int i=0; i<(int)pop.size(); i++)
            mutation(&pop[i]);
        
        // usleep(10000);
        
        if(terminationCondition()) break;
    }
    
    knownDS.lock(); knownDS.data = NULL; knownDS.unlock();
    alienDS.lock(); alienDS.data = NULL; alienDS.unlock();
    bestDS.lock(); bestDS.known = bestDS.alien = NULL; bestDS.ms.clear(); bestDS.unlock();

    {
        static int cnt = 1;
        char filename[32];
        sprintf(filename, "evo-%d.log", cnt++);
        FILE *log = fopen(filename, "w");
        static const int GENtoSTARTwith = 10;
        // for(int i=0; i<(int)bestScores.size(); i++) fprintf(log,"%.8f\n",bestScores[i]);
        for (int i=GENtoSTARTwith; i<(int)logVector.size(); i++)
            for (int j=0; j<LOGperGEN; j++)
                fprintf(log, "%d %.8f\n", i, logVector[i][j]);
        fclose(log);
        info("log written to '%s'", filename);
    }

    return bestEver;
}

Population::Population(const Data *known, const Data *alien)
{
    this->known = known;
    this->alien = alien;
}

/* ------------------------------------------------------------------------ */

static bool fileExists(const char *filename)
{
    struct stat statbuf;
    return stat(filename, &statbuf) == 0 && S_ISREG(statbuf.st_mode);
}

static std::vector<std::string> readPaths(const char *dbfile)
{
    std::vector<std::string> ret;
    
    linereader lrd(dbfile);
    while(lrd.getline())
    {
        int n = lrd.line_len;
        while(n > 0 && isspace(lrd.buffer[n-1])) n--;
        lrd.buffer[n] = '\0';

        for(int i=0; i<n; i++)
            if(lrd.buffer[i] == '\0')
                throw std::runtime_error("failed to parse path list");
        
        char *p = lrd.buffer;
        while(isspace(*p)) p++;

        if(!fileExists(p))
            throw std::runtime_error("failed to parse path list");

        ret.push_back(std::string(p));
    }

    return ret;
}

int main(int argc, char *argv[])
{
    srand(time(0));
    parse_config("evolution.cfg", cfgvars);
    spawn_worker_threads(cfgThreads); /* should detect no. of cpus available */
   
    /* start GUI
     * must go before looking at argc, argv and before
     * calling gui_upload, which is done by Data::build */
    gui_init(&argc, &argv);
    setlocale(LC_ALL, "POSIX"); /* because glib thinks we should use comma as decimal separator... */

    /* check if there's enough command arguments */
    if (argc < 3) {
        fprintf(stderr, "USAGE: ewo [alien image] [file with paths to known images]\n"
                        "       ewo [alien image] [known image] [known image] ...\n");
        return 1;
    }

    /* get known images filenames */
    std::vector<std::string> knownPaths;
    {
        bool gotPaths = false;

        /* first assume that file with known images paths was given */
        if(argc == 3) {
            try {
                knownPaths = readPaths(argv[2]);
                gotPaths = true;
            } catch(std::exception e) {
                warn("failed to read image paths from '%s'", argv[2]);
            }
        }

        /* now assume each argument is separate file to be tested */
        if(!gotPaths)
            for(int i=2; i<argc; i++)
                if(fileExists(argv[i]))
                    knownPaths.push_back(std::string(argv[i]));
                else {
                    fail("file does not exist: '%s'", argv[i]);
                    return 1;
                }
    }
    
    /* show up the displayslots */
    knownDS.activate();
    alienDS.activate();
    bestDS.bind();
    
    /* read alien image */
    gui_status("loading '%s'...", argv[1]);
    Data alien = Data::build(argv[1]);

    /* examine all known images */
    std::vector<std::pair<float, const char *> > results;    
    for(int i=0; i<(int)knownPaths.size(); i++)
    {
        const char *knownPath = knownPaths[i].c_str();
        okay("processing '%s'", knownPath);
        gui_status("loading '%s'...", knownPath);
        Data known = Data::build(knownPath); 
        Agent best = Population(&known, &alien).evolve();
        info("best score was %f", best.target);
        results.push_back(std::make_pair(best.target, knownPath));
    }

    /* print out the verdict */
    std::sort(results.begin(), results.end());
    printf("\n>> the verdict <<\n");
    for(int i = results.size()-1;i>=0;i--)
        printf("%s: %f\n", results[i].second, results[i].first);

    /* and now shut down the GUI, or it will abort() */
    knownDS.deactivate();
    alienDS.deactivate();
    bestDS.deactivate();
    
    return 0;
}
/*void Population::EvaluationJob::runOne(Agent *agent)
{
    const Data *alien = uplink->alien, *known = uplink->known;
    
    const char K = 2;
    char cnts[alien->pois.size()]; 
    memset(cnts, 0, sizeof(cnts));
    
    float sum = 0;
    for(int i=0; i<(int)known->pois.size(); i++)
    {
        if(!agent->mask[i])
            continue;

        Point p = agent->M * known->pois[i];

        int bestidx = -1; float bestdist = 1e+10f;
        for(int j=0; j<(int)alien->pois.size(); j++)
        {
            if(cnts[j] >= K)
                continue;
            float dist = (p - alien->pois[j]).dist();
            if(dist >= bestdist)
                continue;
            bestidx = j;
            bestdist = dist;
        }

        if(bestidx != -1) {
            cnts[bestidx]++;
            sum += bestdist;
        } else {
            warn("cannot match poi %d", i);
            sum += 1e+10f;
        }
    }

    agent->target = -sum / (alien->pois.size() + known->pois.size());
    // now add a penalty for too high flattening
    float sx = agent->sx(), 
          sy = agent->sy();
    float flatCoef = std::max(1.0f, std::max(sx/sy, sy/sx)/cfgMaxScaleRatio);
    agent->target *= expf(flatCoef);
}*/