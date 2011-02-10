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
#include <algorithm>
#include <stdexcept>

#include "util.h"
#include "poi.h"
#include "gui.h"
#include "image.h"
#include "config.h"

/* ------------------------------------------------------------------------ */

static void parsePOIScales(const char *value);
static void parseSurvivalEq(const char *value);

static int cfgThreads;
static int cfgPOISteps;
static std::vector<float> cfgPOIScales;
static float cfgPOIThreshold;
static int cfgPOICount, cfgPOISparseCount;
static int cfgProxMapDetail, cfgProxMapEntries;
static int cfgPopulationSize;
static std::vector<float> cfgSurvivalEq;
static int cfgMaxGenerations;
static float cfgTranslateInit, cfgRotateInit, cfgScaleInit;
static float cfgTranslateProp,  cfgRotateProp,  cfgScaleProp, cfgFlipProp;
static float cfgTranslateDev,   cfgRotateDev,   cfgScaleDev;
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
    { "proxMapDetail",  config_var::INT,       &cfgProxMapDetail },
    { "proxMapEntries", config_var::INT,       &cfgProxMapEntries },
    /* evolution */
    { "populationSize", config_var::INT,       &cfgPopulationSize },
    { "survivalEq",     config_var::CALLBACK,  (void *)&parseSurvivalEq },
    { "maxGenerations", config_var::INT,       &cfgMaxGenerations },
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

class Data
{
    static inline char *makeCacheFilename(const char *filename);
    inline uint32_t checksum() const;
    inline void readCached(const char *filename);
    inline void writeCache(const char *filename) const;

    struct cacheHdr {
        enum { MAGIC = 0x3f0dea79 };
        uint32_t magic;
        uint32_t checksum;
        uint32_t poiCount;
    };

    inline Data() { }
    void doBuild(const char *filename);

public:
    Image raw;
    POIvec dense, sparse;
    ProximityMap prox;
    int originX, originY; /* median of POIs */
    float avgpoidist; /* average sparse POI distance */
    CairoImage raw_ci; /* image for gui */
    CairoImage prox_ci; /* visualization of proximity data */

    static inline Data build(const char *filename) {
        Data ret;
        ret.doBuild(filename);
        return ret;
    }
};

void Data::doBuild(const char *filename)
{
    gui_status("loading '%s'", filename);

    raw = Image::read(filename);

    try {
        readCached(filename);
        sparse = filterPOIs(dense, cfgPOISparseCount);
    }
    catch(std::exception &e)
    {
        warn("failed to read cache: %s", e.what());

        {
            gui_status("loading '%s': looking for POIs", filename);
            Array2D<float> eval = evaluateImage(raw, cfgPOIScales, cfgPOISteps);
            POIvec all = extractPOIs(eval, cfgPOIThreshold);
            dense = filterPOIs(all, cfgPOICount);
            sparse = filterPOIs(dense, cfgPOISparseCount);
        }
    
        {
            gui_status("loading '%s': building proximity map", filename);
            prox.resize(raw.getWidth(), raw.getHeight(),
                        cfgProxMapDetail, cfgProxMapEntries);
            prox.build(sparse);
        }
    
    
        writeCache(filename);
    }

    info("loaded '%s': %d dense pois, %d sparse pois", filename, (int)dense.size(), (int)sparse.size());

    raw_ci = gui_upload(raw);
    prox_ci = gui_upload(prox.visualize());

    {
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

    {
        float sum = 0;
        for(int i=0; i<(int)sparse.size(); i++) {
            /* the second-closest poi, because the first-closesr is us */
            int j = prox.at(sparse[i].x, sparse[i].y)[1]; 
            sum += (sparse[i] - sparse[j]).dist();
        }
        avgpoidist = sum / sparse.size();
    }
    

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
        
        std::vector<Point> ps(alien->sparse.begin(), alien->sparse.end());
        drawDots(ps, 6, alienColor);

        ps.resize(known->sparse.size());
        for(int i=0; i<(int)known->sparse.size(); i++)
            ps[i] = ms[0] * known->sparse[i];
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
    float pointCloudDiameter(Point p) const;
    inline bool terminationCondition() const;

    /* how many specimen will advance to the next generation */
    float getSurvivalRate() const;

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
void Population::EvaluationJob::runOne(Agent *agent)
{
    const Data *alien = uplink->alien, *known = uplink->known;

    /* for each POI from known image
     * we look for closest POI from alien image. moreover, each alien
     * POI can be used at most K times (this is to prevent matching
     * all known POIs with one or two alien POIs). */
    const char K = 2;
    char cnts[alien->sparse.size()]; /* here we count each use of an alien poi */
    memset(cnts, 0, sizeof(cnts));

    float average = 0;
    for(int i=0; i<(int)known->sparse.size(); i++)
    {
        float bestdist = 1e+6;
        int bestidx = -1;

        /* the point we're looking for.
         * because ProximityMap cannot look beyond its own dimensions,
         * we need to clamp point's coordinates to lay within. */
        Point p = agent->M * known->sparse[i];
        p.x = std::max(std::min(p.x, alien->prox.getWidth()-1.f), 0.f);
        p.y = std::max(std::min(p.y, alien->prox.getHeight()-1.f), 0.f);

        /* first try looking the point up using ProximityArray.
         * this will succeed very often (well, depending of number of 
         * entries in the array) */
        for(int j=0; j<alien->prox.getEntries(); j++) {
            int idx = alien->prox.at(p.x, p.y)[j];
            if(cnts[idx] < K) {
                bestidx = idx;
                bestdist = (p - alien->sparse[idx]).dist();
                break;
            }
        } 

        /* if ProximityArray failed, run the full search *
        if(bestidx == -1 && false) {
            for(int j=0; j<(int)alien->sparse.size(); j++)
            {
                float dist = (p - alien->sparse[j]).dist();
                if(cnts[j] < K && dist < bestdist) {
                    bestdist = dist;
                    bestidx = j;
                }
            }
        } */

        if(bestdist < alien->avgpoidist || bestidx == -1)
            average += bestdist;
        else
            average += bestdist*bestdist*bestdist / (alien->avgpoidist * alien->avgpoidist);
        if(bestidx != -1)
            cnts[bestidx] ++;
    }
    
    average /= known->sparse.size();
    agent->target = -average;
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

    if(Random::maybe(cfgTranslateProp))
        a->translate(Random::gaussian(0, cfgTranslateDev) * w,
                     Random::gaussian(0, cfgTranslateDev) * h);

    if(Random::maybe(cfgRotateProp))
        a->rotate(Random::gaussian(0, cfgRotateDev),
                  Random::gaussian(known->originX, cfgOriginDev * w),
                  Random::gaussian(known->originY, cfgOriginDev * h));

    if(Random::maybe(cfgScaleProp))
        a->scale(Random::gaussian(1, cfgScaleDev),
                 Random::gaussian(1, cfgScaleDev),
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
    return generationNumber >= cfgMaxGenerations;
}

/* how many specimen will make it to the next round */
float Population::getSurvivalRate() const
{
    float v = 0, x = std::min(1.f, (generationNumber-1.f) / (cfgMaxGenerations-1.f));
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

    bestDS.lock(); bestDS.known = known; bestDS.alien = alien; bestDS.unlock();
    
    Agent bestEver;
    bestEver.target = -1000000.0f;
    
    const int logPerGen = 10;
    std::vector<std::vector<float> > logVector;

    Timer totalEvolutionTime(CLOCK_PROCESS_CPUTIME_ID);
    totalEvolutionTime.start();

    for(generationNumber = 1; ; generationNumber++)
    {
        evaluate();

        float survivalRate = getSurvivalRate();

		gui_status("gen: %d, survival: %d%%, best fit: target %.2f fitness %f  |  d=(%.0f,%.0f), s=(%.2f,%.2f), a=%.3f",
			       generationNumber, (int)(survivalRate*100.f),
				   pop[0].target, pop[0].fitness,
				   pop[0].dx(),pop[0].dy(),pop[0].sx(),pop[0].sy(),pop[0].alpha()/M_PI*180.0);

        bestDS.lock();
        bestDS.ms.clear();
        for(int i=0; i<(int)pop.size(); i+=std::max(1, (int)pop.size()/30))
            bestDS.ms.push_back(pop[i].M);
        bestDS.unlock();

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
                pop[i] = pop[roulette(survivors)];

        for(int i=0; i<(int)pop.size(); i++)
            mutation(&pop[i]);
        
        if(terminationCondition()) break;
        //usleep(50000);
    }

    debug("total evolution time: %.3f seconds", totalEvolutionTime.end());
    sleep(2);

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
    parse_config("evosingle.cfg", cfgvars);
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
    
    /* read alien image */
    Data alien = Data::build(argv[1]);
    alienDS.set(alien.raw_ci, alien.sparse);
    proxDS.set(alien.prox_ci, alien.sparse);

    /* examine all known images */
    std::vector<std::pair<float, const char *> > results;    
    for(int i=0; i<(int)knownPaths.size(); i++)
    {
        const char *knownPath = knownPaths[i].c_str();
        okay("processing '%s'", knownPath);

        Data known = Data::build(knownPath);
        knownDS.set(known.raw_ci, known.sparse);

        Agent best = Population(&known, &alien).evolve();
        results.push_back(std::make_pair(best.target, knownPath));
        info("best score was %f", best.target);

        knownDS.clear();
    }

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
