#include <cstdlib>
#include <cstdio>
#include <cmath>
#include <cassert>
#include <ctime>
#include <ctype.h>
#include <locale.h>
#include <sys/stat.h>
#include <vector>
#include <bitset>
#include <string>
#include <algorithm>
#include <stdexcept>

#include "util.h"
#include "poi.h"
#include "gui.h"
#include "image.h"
#include "config.h"

#define INF 1e+6f

/* ------------------------------------------------------------------------ */

static void parsePOIScales(const char *value);
static void parseSurvivalEq(const char *value);
static void parseMutationDevEq(const char *value);
static void parseMutationPropEq(const char *value);

static int cfgThreads;
static int cfgPOISteps;
static std::vector<float> cfgPOIScales;
static float cfgPOIThreshold;
static float cfgPOITabuScale;
static int cfgPOICount;
static int cfgPOISparseCount;
static int cfgProxMapDetail, cfgProxMapEntries;
static int cfgMinPois;
static int cfgPopulationSize;
static std::vector<float> cfgSurvivalEq;
static std::vector<float> cfgMutationDevEq;
static std::vector<float> cfgMutationPropEq;
static int cfgStopCondParam, cfgMaxGenerations;
static float cfgTranslateInit, cfgRotateInit, cfgScaleInit;
static float cfgTranslateProp,  cfgRotateProp,  cfgScaleProp, cfgFlipProp;
static float cfgTranslateDev,   cfgRotateDev,   cfgScaleDev;
static float cfgTranslateDev2,  cfgRotateDev2,  cfgScaleDev2;
static float cfgTranslateMean2, cfgRotateMean2, cfgScaleMean2;
static float cfgOriginDev;
static float cfgDEMatingProp, cfgDEMatingCoeff, cfgDEMatingDev;

static struct config_var cfgvars[] = {
    { "threads",        config_var::INT,       &cfgThreads },
    /* poi detection */
    { "poiSteps",       config_var::INT,       &cfgPOISteps },
    { "poiScales",      config_var::CALLBACK,  (void *)&parsePOIScales },
    { "poiCount",       config_var::INT,       &cfgPOICount },
    { "poiSparseCount", config_var::INT,       &cfgPOISparseCount },
    { "poiThreshold",   config_var::FLOAT,     &cfgPOIThreshold },
    { "poiTabuScale",   config_var::FLOAT,     &cfgPOITabuScale },
    { "proxMapDetail",  config_var::INT,       &cfgProxMapDetail },
    { "proxMapEntries", config_var::INT,       &cfgProxMapEntries },
    { "minPois",        config_var::INT,       &cfgMinPois },
    /* evolution */
    { "populationSize", config_var::INT,       &cfgPopulationSize },
    { "stopCondParam",  config_var::INT,       &cfgStopCondParam },
    { "maxGenerations", config_var::INT,       &cfgMaxGenerations },
    /* varying evolution parameters */
    { "survivalEq",     config_var::CALLBACK,  (void *)&parseSurvivalEq },
    { "mutationDevEq",  config_var::CALLBACK,  (void *)&parseMutationDevEq },
    { "mutationPropEq", config_var::CALLBACK,  (void *)&parseMutationPropEq },
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

static float eval(float x, std::vector<float> polyn)
{
    float v = 0;
    for(int i=0; i<(int)polyn.size(); i++)
        v = x*v + polyn[i];
    return v;
}

static void parsePOIScales(const char *value) {
    cfgPOIScales = parseFloatVector(value);
}
static void parseSurvivalEq(const char *value) {
    cfgSurvivalEq = parseFloatVector(value);
}
static void parseMutationDevEq(const char *value) {
    cfgMutationDevEq = parseFloatVector(value);
}
static void parseMutationPropEq(const char *value) {
    cfgMutationPropEq = parseFloatVector(value);
}

/* ------------------------------------------------------------------------ */

static Timer globalEvolutionTmr, globalBuildTmr;

/* ------------------------------------------------------------------------ */

class Data
{
    static inline char *makeCacheFilename(const char *filename);
    inline uint32_t checksum() const;
    inline void readCached(const char *filename);
    inline void writeCache(const char *filename) const;

    struct cacheHdr {
        enum { MAGIC = 0xc9981ad4 };
        uint32_t magic;
        uint32_t checksum;
        uint32_t poiCount;
        uint32_t originX, originY;
    };

    inline Data() { }
    void doBuild(const char *filename, bool setTabuScale);

public:
    Image raw;
    POIvec dense, sparse;
    ProximityMap prox;
    int originX, originY; /* median of POIs */
    CairoImage raw_ci; /* image for gui */
    CairoImage prox_ci; /* visualization of proximity data */

    static inline Data build(const char *filename, bool setTabuScale = false) {
        Data ret;
        ret.doBuild(filename, setTabuScale);
        return ret;
    }
};

void Data::doBuild(const char *filename, bool setTabuScale /*= false*/)
{
    gui_status("loading '%s'", filename);

    raw = Image::read(filename);

    try {
        readCached(filename);
        if (setTabuScale) {
            float foundTabu = -1.0f;
            sparse = filterPOIs(dense, cfgPOISparseCount, &foundTabu);
            debug("sparse size. desired: %d, got: %d, setting tabu as %.3f\n", cfgPOISparseCount, sparse.size(), foundTabu);
            cfgPOITabuScale = foundTabu;
        }
        else
            sparse = filterPOIs(dense, cfgPOICount, cfgPOITabuScale, Matrix());
    }
    catch(std::exception &e)
    {
        warn("failed to read cache: %s", e.what());

        {
            /* POI finding */
            gui_status("loading '%s': looking for POIs", filename);
            Array2D<float> eval = evaluateImage(raw, cfgPOIScales, cfgPOISteps);
            POIvec all = extractPOIs(eval, cfgPOIThreshold);
            dense = filterPOIs(all, cfgPOICount);
            if (setTabuScale) {
                float foundTabu = -1.0f;
                sparse = filterPOIs(dense, cfgPOISparseCount, &foundTabu);
                debug("sparse size. desired: %d, got: %d, setting tabu as %.3f\n", cfgPOISparseCount, sparse.size(), foundTabu);
                cfgPOITabuScale = foundTabu;
                POIvec sparse2 = filterPOIs(dense, cfgPOICount, cfgPOITabuScale, Matrix());
                debug("sparse sizes: %d vs %d\n", sparse.size(), sparse2.size());
            }
            else
                sparse = filterPOIs(dense, cfgPOICount, cfgPOITabuScale, Matrix());
        }
    
        {
            /* build proximity map */
            gui_status("loading '%s': building proximity map", filename);
            prox.resize(raw.getWidth(), raw.getHeight(),
                        cfgProxMapDetail, cfgProxMapEntries);
            prox.build(sparse);
        }
    
        {
            /* find origin */
            int n = dense.size();
            float v[n];
            
            for(int i=0; i<n; i++)
                v[i] = dense[i].x;
            std::sort(v, v+n);
            originX = v[n/2];

            for(int i=0; i<n; i++)
                v[i] = dense[i].y;
            std::sort(v, v+n);
            originY = v[n/2];
        }
    
        writeCache(filename);
    }

    info("loaded '%s': %d dense pois, %d sparse pois", filename, dense.size(), sparse.size());

    raw_ci = gui_upload(raw);
    prox_ci = gui_upload(prox.visualize());

    progress(-1);
}

static inline uint32_t float2u32(float v) {
    union { float x; uint32_t y; } aa;
    aa.x = v;
    return aa.y;
}
uint32_t Data::checksum() const
{
    uint32_t ret = raw.checksum();
    ret ^= (cfgPOISteps*909091) ^
           (cfgPOICount*100000007) ^
           (cfgProxMapDetail*43066337) ^
           (cfgProxMapEntries*59284223);
    ret ^= float2u32(cfgPOIThreshold) ^
           float2u32(cfgPOITabuScale);
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

    dense.resize(hdr.poiCount);
    if(fread(&dense[0], sizeof(POI),hdr.poiCount, f) != hdr.poiCount) {
        dense.clear(); fclose(f);
        throw std::runtime_error("failed to read POIs");
    }

    int proxsize = raw.getWidth() * cfgProxMapDetail *
                   raw.getHeight() * cfgProxMapDetail * 
                   cfgProxMapEntries * sizeof(ProximityMap::poiid_t);
    prox.resize(raw.getWidth(), raw.getHeight(),
                cfgProxMapDetail, cfgProxMapEntries);
    if(fread(prox.at(0,0), proxsize,1, f) != 1) {
        dense.clear();
        prox.resize(0,0,0,0);
        throw std::runtime_error("failed to read proximity map");
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
        throw std::runtime_error("failed to open cache file");

    cacheHdr hdr;
    hdr.magic = cacheHdr::MAGIC;
    hdr.checksum = checksum();
    hdr.poiCount = dense.size();
    hdr.originX = originX;
    hdr.originY = originY;

    int proxsize = prox.getWidth() * prox.getDetail() *
                   prox.getHeight() * prox.getDetail() * 
                   prox.getEntries() * sizeof(ProximityMap::poiid_t);
    if(fwrite(&hdr, sizeof(hdr),1, f) != 1 ||
       fwrite(&dense[0], sizeof(POI),dense.size(), f) != dense.size() ||
       fwrite(prox.at(0,0), proxsize,1, f) != 1) {
        fclose(f);
        throw std::runtime_error("failed to write cache file");
    }
    
    fclose(f);
}

/* ------------------------------------------------------------------------ */

class ImageDisplaySlot : public DisplaySlot
{
public:
    CairoImage ci;
    std::vector<Point> ps;
    rgba color;
    float dotsize;
    bool empty;

    inline ImageDisplaySlot(const char *name, rgba color, float dotsize = 6)
        : DisplaySlot(name), color(color), dotsize(dotsize), empty(true) { }
    virtual ~ImageDisplaySlot() { }

    virtual void draw()
    {
        if(empty)
            resize(0,0);
        else {
            resize(ci.getWidth(), ci.getHeight());
            drawImage(ci);
            drawDots(ps, dotsize, color);
        }
    }

    inline void set(CairoImage ci, const POIvec &pois)
    {
        lock();
        this->ci = ci;
        ps.resize(pois.size());
        std::copy(pois.begin(), pois.end(), ps.begin());
        empty = false;
        unlock();
    }
    inline void clear()
    {
        lock();
        ci = CairoImage();
        ps.clear();
        empty = true;
        unlock();
    }
};

class FitDisplaySlot : public DisplaySlot
{
public:
    const Data *known, *alien;
    rgba knownColor, alienColor, silhouetteColor;
    std::vector<Matrix> ms;
    std::bitset<256> mask;

    inline FitDisplaySlot(const char *name) : DisplaySlot(name) 
    {
        known = alien = NULL;
        knownColor   = rgba(.1, 1, .1,.5);
        alienColor         = rgba(1, .3, .1,.5);
        silhouetteColor    = rgba(.2,.5, 1, .3);
    }

    virtual ~FitDisplaySlot() { }

    virtual void draw()
    {
        if(!known || !alien || ms.size() == 0)
            return;

/*      Timer tim;
        tim.start(); */

        resize(alien->raw.getWidth(), alien->raw.getHeight());
        drawImage(alien->raw_ci);
        drawDifference(known->raw_ci, ms[0]);

        drawSilhouettes(ms, known->raw.getWidth(), known->raw.getHeight(), silhouetteColor);
        
        std::vector<Point> ps, psnon;
        for (int i=0; i<(int)alien->sparse.size(); i++) {
            if (mask[i])
                ps.push_back(alien->sparse[i]);
            else
                psnon.push_back(alien->sparse[i]);
        }
        drawDots(ps, 6, alienColor);
        drawDots(psnon, 6, silhouetteColor);
            
        
        (alien->sparse.begin(), alien->sparse.end());
        drawDots(ps, 6, alienColor);

        POIvec knownsparse = filterPOIs(known->dense, cfgPOICount, cfgPOITabuScale, ms[0]);
        ps.resize(knownsparse.size());
        std::copy(knownsparse.begin(), knownsparse.end(), ps.begin());
        drawDots(ps,6, knownColor);
        
/*      debug("drawing took %.6f ms", tim.end()*1000.f); */
    }
};

/* those are created statically, however they will only initialize
 * themselves upon first action, which must occur after gtk_gui_init. */

static ImageDisplaySlot knownDS("known image", rgba(0.1,1,0.1,0.5)),
                        alienDS("alien image", rgba(1,0.3,0.1,0.5)),
                        proxDS("alien proximity map", rgba(1,1,1,1), 3);
static FitDisplaySlot   bestDS("best fit");

/* ------------------------------------------------------------------------ */

class Agent
{
public:
    Matrix M; /* the transformation matrix */
    float target; /* target function */
    float fitness; /* population-wide fitness factor */

    /*TEST*/std::bitset<256> mask;
    /* agents ordering */
    inline bool operator<(const Agent &A) const {
        return fitness > A.fitness;
    }
    
    /* transformation matrix decomposition */
    inline float dx() const { return M[0][2]; }
    inline float dy() const { return M[1][2]; }
    inline float sx() const { return sqrtf(M[0][0]*M[0][0]+M[0][1]*M[0][1]); }
    inline float sy() const { return sqrtf(M[1][0]*M[1][0]+M[1][1]*M[1][1]); }
    inline float alpha() const { float s = sx(); return atan2f(M[0][1]/s, M[0][0]/s); }

    inline void translate(float dx, float dy);
    inline void rotate(float angle, float ox, float oy);
    inline void scale(float sx, float sy, float ox, float oy);    
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
    
    static float distance(const Data *base, const POIvec &queryvec, EvaluationJob *EJob);
    
    inline void evaluate();
    int fullsearches, operations;

    /* roulette selection */
    int roulette(int n);

    /* mutations and matings */
    inline void makeRandom(Agent *a);
    inline void mutation(Agent *a);
    inline void deMating(Agent *a, const Agent *p, const Agent *q, const Agent *r);

    /* one needs to know when to stop! */
    inline bool terminationCondition() const;

    /* how many specimen will advance to the next generation */
    inline float getSurvivalRate() const {return eval((generationNumber-1) / (cfgMaxGenerations-1), cfgSurvivalEq); }
    
    /* mutation deviation scaling */
    inline float getMutationDev() const { return eval((generationNumber-1) / (cfgMaxGenerations-1), cfgMutationDevEq); }
    
    /* mutation probability scaling */
    inline float getMutationProp() const { return eval((generationNumber-1) / (cfgMaxGenerations-1), cfgMutationPropEq); }

public:
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
/* auxillary functions for runOne */
static inline float max3(float a, float b, float c) { return std::max(std::max(a,b),c); }
static inline FourFloats vectorSpan(const POIvec &v)
{
    float minx=INF,miny=INF,maxx=-INF,maxy=-INF;
    for (int i=0; i<(int)v.size(); i++) {
        minx = std::min(minx, v[i].x);
        maxx = std::max(maxx, v[i].x);
        miny = std::min(miny, v[i].y);
        maxy = std::max(maxy, v[i].y);
    }
    return FourFloats(minx,maxx,miny,maxy);
}
static inline float vectorSpanScalar(const POIvec &v) {
    FourFloats FF = vectorSpan(v);
    return Point(FF.xy-FF.xx+1.f,FF.yy-FF.yx+1.f).disteval();
}

float Population::distance(const Data *base, const POIvec &queryvec, EvaluationJob *EJob)
{
    if (queryvec.empty())
        return 0;
    const char K = 100;
    char cnts[base->sparse.size()]; /* here we count each use of an alien poi */
    memset(cnts, 0, sizeof(cnts));
    
    int fullsearches = 0, operations = 0;
    float sum = 0;
    for(int i=0; i<(int)queryvec.size(); i++)
    {
        /* the point we're looking for.
         * because ProximityMap cannot look beyond its own dimensions,
         * we need to clamp point's coordinates to lay within. */
        Point p = queryvec[i];
        p.x = std::min(std::max(p.x, 0.f), base->prox.getWidth()-1.f);
        p.y = std::min(std::max(p.y, 0.f), base->prox.getHeight()-1.f);

        /* first try looking the point up using ProximityArray.
         * this will succeed very often (well, depending of number of 
         * entries in the array) */
        int bestidx = -1;
        for(int j=0; j<base->prox.getEntries(); j++) {
            int idx = base->prox.at(p.x, p.y)[j];
            operations ++;
            if(cnts[idx] < K) {
                bestidx = idx;
                break;
            }
        } 

        /* if ProximityArray failed, run the full search */
        if(bestidx == -1) {
            fullsearches++;
            float bestdist = INF;
            for(int j=0; j<(int)base->sparse.size(); j++)
            {
                operations ++;
                float dist = (p - base->sparse[j]).disteval();
                if(cnts[j] < K && dist < bestdist) {
                    bestdist = dist;
                    bestidx = j;
                }
            }
        }

        if(bestidx == -1) {
            //warn("cannot match poi %f,%f", p.x, p.y);
            sum += INF;
        } else {
            cnts[bestidx] ++;
            sum += (p - base->sparse[bestidx]).disteval();
        }
    }
    int nzeroSum = 0, nzeroCnt = 0;
    for (int i=0; i<(int)base->sparse.size(); i++)
        if (cnts[i])
            nzeroCnt ++,
            nzeroSum += cnts[i];
    
    /* warning! this is not thread-safe, results may be wrong */
    EJob->uplink->fullsearches += fullsearches;
    EJob->uplink->operations += operations;
    return sum * exp((float)nzeroSum/nzeroCnt-1.f); // vectorSpanScalar(queryvec);
}
    

void Population::EvaluationJob::runOne(Agent *agent)
{
#define DEBUG false
    const Data *alien = uplink->alien, *known = uplink->known;

    /* for each POI from known image [--- TODO: update ---]
     * we look for closest POI from alien image. moreover, each alien
     * POI can be used at most K times (this is to prevent matching
     * all known POIs with one or two alien POIs). */

    //{
        /* p ------- q
         * |       /
         * |     /
         * |   /
         * | /
         * r           */
        /* attempt to cut out too "flattening" agents AND too "resizing" ones */
        Point p = agent->M * Point(0,0),
              q = agent->M * Point(1,0),
              r = agent->M * Point(0,1);
        float d1 = (p-q).dist(),
              d2 = (p-r).dist(),
              d3 = (q-r).dist() / 1.41f;
        float ratioExtrem = max3(
            std::max(d1/d2, d2/d1),
            std::max(d1/d3, d3/d1),
            std::max(d2/d3, d3/d2));
        float scaleExtrem = max3(
            std::max(d1, 1.f/d1),
            std::max(d2, 1.f/d2),
            std::max(d3, 1.f/d3));
        if (DEBUG)
            debug("0 0 -> %.2f %.2f, 0 1 -> %.2f %.2f, 1 0 -> %.2f %.2f. dists: %.3f %.3f %.3f", p.x, p.y, r.x, r.y, q.x, q.y, d1, d2, d3);
        if(ratioExtrem > 1.5f || scaleExtrem > 4.0f) {
            // warn("agent has too high ratio or scale: %.3f %.3f\n", ratioExtrem, scaleExtrem);
            agent->target = -INF;
            return;
        }
    //}
        
    POIvec knownsparse = filterPOIs(known->dense, cfgPOICount, cfgPOITabuScale, agent->M);
    if((int)knownsparse.size() < cfgMinPois) {
        //warn("too little pois found: %d", knownsparse.size());
        agent->target = -INF;
        return ;
    }
    
    agent->mask.reset();
    FourFloats FF = vectorSpan(knownsparse);
    Matrix revM = agent->M.inverse();
    POIvec activealien;
    for (int i=0; i<(int)alien->sparse.size(); i++) {
        POI p = revM * alien->sparse[i];
        if (FF.inside(p)) {
            activealien.push_back(p);
            agent->mask.set(i);
        }
    }
    
    /* how about the penalty for too greedy matching ?? 
     * basically, there are a few solutions:
        * set an upper bound for number of known pois matched to one alien point <- dont' think good idea
        * introduce a penalty for "too flattening" matrices <- doesn't work in my opinion 
        * introduce a penalty for too high average of matched points */
    /* no, no, no, doesn't work! the problem is somewhere else and, unfortunately, i know where */
    
    float dist1 = distance(alien, knownsparse, this),
          dist2 = distance(known, activealien, this);
          
    if (DEBUG)
        debug("d=(%.2f,%.2f) s=(%.2f,%.2f) a=%.2f, pois=%d. rextrem = %.2f, sextrem = %.2f. res=%.3f, span=%.2f\n", 
          agent->dx(),agent->dy(),agent->sx(),agent->sy(),agent->alpha()/M_PI*180.,knownsparse.size(),ratioExtrem,scaleExtrem,dist1,
          vectorSpanScalar(knownsparse)
         );
    
    // debug("%.2f (%.3f) %.2f (%.3f)\n", dist1, dist1/knownsparse.size(), dist2, dist2/activealien.size());
    agent->target = -(dist1/* + dist2*/) / (knownsparse.size() /*+ activealien.size()*/);
    agent->target = std::max(agent->target, -INF);
    
}
void Population::evaluate()
{
    const int evalBatch = 100;
    int nJobs = (pop.size() + evalBatch - 1) / evalBatch;
    EvaluationJob jobs[nJobs];
    Completion c;
    
    for(int i=0; i<nJobs; i++) {
        jobs[i].completion = &c;
        jobs[i].uplink = this;
        jobs[i].start = evalBatch*i;
        jobs[i].end = std::min((int)pop.size(), evalBatch*(i+1));
    }

    fullsearches = operations = 0;
    for(int i=0; i<nJobs; i++)
        aq->queue(&jobs[i]);
    c.wait();
    debug("  did at least %d fullsearches and %d operations", fullsearches, operations);

    float minTarget = INF;
    for(int i=0; i<(int)pop.size(); i++) {
        // debug("%d -> %.3f", i, pop[i].target);
        minTarget = std::min(minTarget, pop[i].target);
    }
    
    // debug("min target = %.3f", minTarget);

    float denominator = 0;
    for(int i=0; i<(int)pop.size(); i++)
        denominator += pop[i].target - minTarget;
    
    // debug("denominator = %.5f", denominator);
    
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
    a->M = Matrix();
    a->translate(
            Random::real(cfgTranslateInit) * alien->raw.getWidth(),
            Random::real(cfgTranslateInit) * alien->raw.getHeight());
    a->rotate(Random::real(cfgRotateInit),
            known->originX, known->originY);
    a->scale(1.f+Random::real(cfgScaleInit),1.f+Random::real(cfgScaleInit),
            known->originX, known->originY);
}
void Population::mutation(Agent *a)
{
    float w = known->raw.getWidth(),
          h = known->raw.getHeight();
          
    float sdev = getMutationDev(); /* mutation deviation scale */
    float pdev = getMutationProp();
    
    if(Random::maybe(cfgTranslateProp*pdev))
        a->translate(Random::trigauss(0, cfgTranslateDev * sdev, cfgTranslateMean2, cfgTranslateDev2 * sdev) * w,
                     Random::trigauss(0, cfgTranslateDev * sdev, cfgTranslateMean2, cfgTranslateDev2 * sdev) * h);

    if(Random::maybe(cfgRotateProp*pdev))
        a->rotate(Random::trigauss(0, cfgRotateDev * sdev, cfgRotateMean2, cfgRotateDev2 * sdev),
                  Random::gaussian(known->originX, cfgOriginDev * w * sdev),
                  Random::gaussian(known->originY, cfgOriginDev * h * sdev));

    if(Random::maybe(cfgScaleProp*pdev))
        a->scale(Random::trigauss(1, cfgScaleDev * sdev, cfgScaleMean2, cfgScaleDev2 * sdev),
                 Random::trigauss(1, cfgScaleDev * sdev, cfgScaleMean2, cfgScaleDev2 * sdev),
                 Random::gaussian(known->originX, cfgOriginDev * w * sdev),
                 Random::gaussian(known->originY, cfgOriginDev * h * sdev));

    if(Random::maybe(cfgFlipProp*pdev))
    {
        float rangle = Random::real(2.f*M_PI),
              rx = Random::gaussian(known->originX, cfgOriginDev*w * sdev),
              ry = Random::gaussian(known->originY, cfgOriginDev*h * sdev);
        
        a->rotate(rangle, rx, ry);
        a->scale(-1, 1, rx, ry);
        a->rotate(-rangle, rx, ry);
    }
}

/* --- differential evolution mating */
void Population::deMating(Agent *a, const Agent *p, const Agent *q, const Agent *r)
{
    float factor = fabs(Random::gaussian(cfgDEMatingCoeff, cfgDEMatingDev));
    if(q->fitness < r->fitness) std::swap(q,r);
    a->M = p->M + (q->M - r->M) * factor;
}

/* this tells us when to stop */
bool Population::terminationCondition() const
{
    if(generationNumber > cfgMaxGenerations)
        return true;

    return false;
    const int K = cfgStopCondParam;
    if ((int)bestScores.size() < 2*K)
        return false;

    /* jesli przez ostatnie K pokolen nie stalo sie nic ciekawszego niz podczas poprzednich K */
    float max1 = *std::max_element(bestScores.end()-K,   bestScores.end()),
          max2 = *std::max_element(bestScores.end()-2*K, bestScores.end()-K);

    return max2 >= max1;
}
    
/* --- the so called main loop */
Agent Population::evolve()
{
    globalEvolutionTmr.resume();
    pop.resize(cfgPopulationSize);
    for(int i=0; i<(int)pop.size(); i++)
        makeRandom(&pop[i]);
    globalEvolutionTmr.pause();
    
    bestDS.lock(); bestDS.known = known; bestDS.alien = alien; bestDS.unlock();
    
    Agent bestEver;
    bestEver.target = -1000000.0f;
    
    const int logPerGen = 10;
    std::vector<std::vector<float> > logVector;

    Timer totalEvolutionTime(CLOCK_PROCESS_CPUTIME_ID);
    totalEvolutionTime.start();
    float ctime = .0f;
    
    globalEvolutionTmr.resume();
    for(generationNumber = 1; ; generationNumber++)
    {
        debug("start generation %d", generationNumber);
        
        evaluate();
        debug("  evaluation time: %.5f", totalEvolutionTime.end()-ctime); ctime = totalEvolutionTime.end();
        

        float survivalRate = getSurvivalRate();

		gui_status("gen: %d, survival: %d%%, best fit: target %.2f fitness %f  |  d=(%.0f,%.0f), s=(%.2f,%.2f), a=%.3f",
			       generationNumber, (int)(survivalRate*100.f),
				   pop[0].target, pop[0].fitness,
				   pop[0].dx(),pop[0].dy(),pop[0].sx(),pop[0].sy(),pop[0].alpha()/M_PI*180.0);

        bestDS.lock();
        bestDS.ms.clear();
        for(int i=0; i<(int)pop.size(); i+=std::max(1, (int)pop.size()/30))
            bestDS.ms.push_back(pop[i].M);
        bestDS.mask = pop[0].mask;
        bestDS.unlock();
        
        // debug("  drawing time: %.5f", totalEvolutionTime.end()-ctime); ctime = totalEvolutionTime.end();
        
        bestScores.push_back(pop[0].target);
        if (bestEver.target < pop[0].target)
            bestEver = pop[0];
        
        logVector.push_back(std::vector<float>(logPerGen));
        logVector.back()[0] = pop[0].target;
        for (int i=1; i<logPerGen; i++)
            logVector.back()[i] = pop[rand()%pop.size()/2].target;
        
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
                pop[i] = pop[rand()%survivors]; /*CHANGE*/
            
        // debug("  DE mating time: %.5f", totalEvolutionTime.end()-ctime); ctime = totalEvolutionTime.end();
        
        for(int i=0; i<(int)pop.size(); i++)
            mutation(&pop[i]);
        
        debug("  evolution operations time: %.5f", totalEvolutionTime.end()-ctime); ctime = totalEvolutionTime.end();
        
        if(terminationCondition()) break;
    }

    debug("total evolution time: %.3f seconds", totalEvolutionTime.end());
    // sleep(5);
    globalEvolutionTmr.pause();
    
    bestDS.lock(); bestDS.known = bestDS.alien = NULL; bestDS.ms.clear(); bestDS.unlock();

    {
        static int cnt = 1;
        char filename[32];
        sprintf(filename, "evo-%d.log", cnt++);
        FILE *log = fopen(filename, "w");

        /* wykresy w gnuplocie mozna robic z pewnego przedzialu danych:
         * gnuplot> set xrange [10:80] */
        for (int i=0; i<(int)logVector.size(); i++)
            for (int j=0; j<logPerGen; j++)
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
    progress = gui_progress;
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
    proxDS.activate();
    bestDS.bind();
    
    globalEvolutionTmr.start();
    globalEvolutionTmr.pause();
    
    /* read alien image */
    globalBuildTmr.start();
    Data alien = Data::build(argv[1], true); /*setting up tabu scale! */
    globalBuildTmr.pause();
    
    alienDS.set(alien.raw_ci, alien.sparse);
    proxDS.set(alien.prox_ci, alien.sparse);
    /* examine all known images */
    std::vector<std::pair<float, const char *> > results;    
    for(int i=0; i<(int)knownPaths.size(); i++)
    {
        const char *knownPath = knownPaths[i].c_str();
        okay("processing '%s'", knownPath);
        
        globalBuildTmr.resume();
        Data known = Data::build(knownPath);
        globalBuildTmr.pause();
        knownDS.set(known.raw_ci, known.dense);
        
        Agent best = Population(&known, &alien).evolve();
        results.push_back(std::make_pair(best.target, knownPath));
        info("best score was %f", best.target);

        knownDS.clear();
    }
    
    printf("global build time = %.5f\nglobal evolution time = %.5f\n", globalBuildTmr.end(), globalEvolutionTmr.end());
    
    /* print out the verdict */
    std::sort(results.begin(), results.end());
    printf("\n>> the verdict <<\n");
    for(int i = results.size()-1;i>=0;i--)
        printf("%s: %f\n", results[i].second, results[i].first);

    /* and now shut down the GUI, or it will abort() */
    knownDS.deactivate();
    alienDS.deactivate();
    proxDS.deactivate();
    bestDS.deactivate();
    
    return 0;
}
