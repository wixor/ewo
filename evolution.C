#include <cstdlib>
#include <cstdio>
#include <cmath>
#include <cassert>
#include <ctype.h>
#include <vector>
#include <string>
#include <algorithm>
#include <stdexcept>

#include "poi.h"
#include "evolution.h"
#include "workers.h"
#include "gui.h"
#include "image.h"
#include "config.h"


#define debug(fmt, ...) fprintf(stderr, fmt, ## __VA_ARGS__)

/* ------------------------------------------------------------------------ */

static void parsePOIScales(const char *value);

static int cfgThreads;
static int cfgPOISteps;
static std::vector<float> cfgPOIScales;
static float cfgPOIThreshold;
static float cfgPOITabuParam;
static int cfgPOICount;
static int cfgPopulationSize;
static float cfgSurvivalRate;
static float cfgTranslateInit, cfgRotateInit, cfgScaleInit;
static float cfgTranslateProp, cfgRotateProp, cfgScaleProp, cfgFlipProp;
static float cfgTranslateDev, cfgRotateDev, cfgScaleDev;
static float cfgOriginDev;
static float cfgDEMatingProp, cfgDEMatingCoeff, cfgDEMatingDev;

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
    { "survivalRate",   config_var::FLOAT,     &cfgSurvivalRate },
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

static void parsePOIScales(const char *value)
{
    while(*value)
    {
        char *end;
        float v = strtof(value, &end);
        if(end == value)
            throw std::runtime_error("failed to parse scales");
        
        cfgPOIScales.push_back(v);

        while(isspace(*end)) end++;
        if(*end == ',') end++;
        while(isspace(*end)) end++;

        value = end;
    }
}

/* ------------------------------------------------------------------------ */

class Random
{
public:
    static inline bool coin() {
        return rand()&1;
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
    static inline float gaussian(float mean, float deviation) {
        /* Box-Muller transform ftw ;) */
        float p = positive(1), q = positive(1);
        float g = sqrtf(-2.0f * logf(p)) * cosf(2.0f*M_PI*q);
        return g*deviation + mean;
    }
};

/* ------------------------------------------------------------------------ */

class Data
{
    inline void findOrigin();
    inline void findPOIs();

    static inline std::string makeCacheFilename(const std::string &filename);
    inline uint32_t checksum() const;
    inline bool readCached(const char *filename);
    inline void writeCache(const char *filename) const;

    struct cacheHdr {
        enum { MAGIC = 0xfc83dea1 };
        uint32_t magic;
        uint32_t checksum;
        uint32_t poiCount;
        uint32_t originX, originY;
    };

    inline Data() { }

public:
    char name[64];
    char cathegory[64];
    Image *raw;
    std::vector<POI> pois;
    int originX, originY; /* median of POIs */
    CairoImage compimg; /* image for compositing */

    static Data build(const char *filename);

    std::vector<POI> transformPOIs(const Matrix33 &M) const;
};

Data Data::build(const char *filename)
{
    Data ret;
    ret.raw = new Image(0,0);
    (*ret.raw) = Image::readPGM(filename);

    if(!ret.readCached(filename)) {
        debug("...finding POIs\n");
        ret.findPOIs();
        debug("...finding origin\n");
        ret.findOrigin();
        debug("...writing cache\n");
        ret.writeCache(filename);
    }
    debug("preparing composite\n");
    Composite::prepare(*ret.raw, &ret.compimg);

    debug("done\n");
    return ret;
}

void Data::findPOIs()
{
    Array2D<float> eval = ::evaluateImage(*raw, cfgPOISteps, cfgPOIScales);
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

std::string Data::makeCacheFilename(const std::string &filename)
{
    if(filename.size() < 4 || filename.compare(filename.size()-4, 4, ".pgm") != 0) {
        fprintf(stderr, "WARNING: unknown image file extension\n");
        return NULL;
    }

    std::string ret = filename;
    ret[ret.size()-3] = 'd'; ret[ret.size()-2] = 'a'; ret[ret.size()-1] = 't';
    return ret;
}

static inline uint32_t float2u32(float v) {
    union { float x; uint32_t y; } aa;
    aa.x = v;
    return aa.y;
}
uint32_t Data::checksum() const
{
    uint32_t ret = raw->checksum();
    ret ^= cfgPOISteps ^ cfgPOICount;
    ret ^= float2u32(cfgPOIThreshold);
    for(int i=0; i<(int)cfgPOIScales.size(); i++)
        ret ^= float2u32(cfgPOIScales[i]);
    return ret;
}

bool Data::readCached(const char *filename)
{
    FILE *f = fopen(makeCacheFilename(std::string(filename)).c_str(), "rb");
    if(!f) {
        fprintf(stderr, "failed to open cache file\n");
        return false;
    }

    struct cacheHdr hdr;
    if(fread(&hdr, sizeof(hdr),1, f) != 1) {
        fprintf(stderr, "WARNING: corrupted cache file header\n");
        fclose(f); return false;
    }

    if(hdr.magic != cacheHdr::MAGIC) {
        fprintf(stderr, "WARNING: invalid cache file magic (found %08x, expected %08x)\n", hdr.magic, cacheHdr::MAGIC);
        fclose(f); return false;
    }

    if(hdr.checksum != checksum()) {
        fprintf(stderr, "cache doesn't match image, propably stale cache file\n");
        fclose(f); return false;
    }

    pois.resize(hdr.poiCount);
    if(fread(&pois[0], sizeof(POI),hdr.poiCount, f) != hdr.poiCount) {
        fprintf(stderr, "WARNING: failed to read POIs from cache\n");
        pois.clear(); fclose(f); return false;
    }

    originX = hdr.originX;
    originY = hdr.originY;

    fclose(f);

    fprintf(stderr, "cache read successfully\n");
    return true;
}

void Data::writeCache(const char *filename) const
{
    FILE *f = fopen(makeCacheFilename(std::string(filename)).c_str(), "wb");
    if(!f) throw std::runtime_error("failed to write cache file");

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

    fprintf(stderr, "cache written successfully\n");
    fclose(f);
}

std::vector<POI> Data::transformPOIs(const Matrix33 &M) const
{
    std::vector<POI> xfmd(pois.size());
    for(int i=0; i<(int)pois.size(); i++)
        xfmd[i] = M * pois[i];
    return xfmd;
}

/* ------------------------------------------------------------------------ */

class Agent
{
public:
    Matrix33 M; /* the transformation matrix */
    float distance; /* point cloud distance */ 
    float difference; /* image difference metric */
    float target; /* target function, computed from the above */
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
    inline float alfa() const { float s = sx(); return atan2f(M[0][1]/s, M[0][0]/s); }

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
    M = Matrix33::translation(dx,dy)
      * M;
}
void Agent::rotate(float angle, float ox, float oy) {
    POI origin = M * POI(ox,oy,0);
    M = Matrix33::translation(origin.x,origin.y)
      * Matrix33::rotation(angle)
      * Matrix33::translation(-origin.x,-origin.y)
      * M;
}
void Agent::scale(float sx, float sy, float ox, float oy) {
    POI origin = M * POI(ox,oy,0);
    M = Matrix33::translation(origin.x,origin.y)
      * Matrix33::scaling(sx, sy)
      * Matrix33::translation(-origin.x,-origin.y)
      * M;
}

/* ------------------------------------------------------------------------ */

class Population
{
    /* we're trying to match the Known image to the Alien image */
    const Data *known, *alien;

    /* our lovely little things */
    std::vector<Agent> pop;

    /* population evaluation (multi-threaded) */
    class EvaluationJob : public AsyncJob
    {
    public:
        Population *uplink;
        Agent *agent;
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
    inline static float stopWLOG(const std::vector<float>& v);

public:
    Population(const Data *known, const Data *alien);
    Agent evolve();
};

/* --- population evaluation */
Population::EvaluationJob::~EvaluationJob() {
    /* empty */
}

/* dla każdego punktu z a najbliższy mu z b */
static float matchPOIs(const std::vector<POI> &a, const std::vector<POI> &b)
{
    float sum = 0;
    for(int i=0; i<(int)a.size(); i++)
    {
        float best = 1e+30;
        for(int j=0; j<(int)b.size(); j++)
            best = std::min(best, POI::dist(a[i], b[j]));
        sum += best;
    }
    return sum;
}
void Population::EvaluationJob::run()
{
    const Data *alien = uplink->alien, *known = uplink->known;

    std::vector<POI> xformed = known->transformPOIs(agent->M);
    agent->distance = -matchPOIs(xformed, alien->pois)
                      -matchPOIs(alien->pois, xformed);
    agent->distance /= alien->pois.size() + known->pois.size();

    agent->difference = 0;//-Composite::difference(known->compimg, *alien->raw, agent->M);
    
    agent->target = agent->distance;
}
void Population::evaluate()
{
    EvaluationJob jobs[pop.size()];
    Completion c;
    
    for(int i=0; i<(int)pop.size(); i++) {
        jobs[i].completion = &c;
        jobs[i].agent = &pop[i];
        jobs[i].uplink = this;
        aq->queue(&jobs[i]);
    }

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

    fprintf(stderr, "WARNING: roulette selection failed, residual %e\n", y);
    return 0;
}

/* --- mutations */
void Population::makeRandom(Agent *a)
{
    a->M = Matrix33();
    a->translate(
            Random::real(cfgTranslateInit) * known->raw->getWidth(),
            Random::real(cfgTranslateInit) * known->raw->getHeight());
    a->rotate(Random::real(cfgRotateInit),
            known->originX, known->originY);
    a->scale(1.f+Random::real(cfgScaleInit),1.f+Random::real(cfgScaleInit),
            known->originX, known->originY);
}
void Population::mutation(Agent *a)
{
    float w = known->raw->getWidth(),
          h = known->raw->getHeight();

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
        a->scale(-1, 1,
                 Random::gaussian(known->originX, cfgOriginDev * w),
                 Random::gaussian(known->originY, cfgOriginDev * h));
    if(Random::maybe(cfgFlipProp))
        a->scale(1, -1,
                 Random::gaussian(known->originX, cfgOriginDev * w),
                 Random::gaussian(known->originY, cfgOriginDev * h));
}

/* --- differential evolution mating */
void Population::deMating(Agent *a, const Agent *p, const Agent *q, const Agent *r)
{
    float factor = fabs(Random::gaussian(cfgDEMatingCoeff, cfgDEMatingDev));
    if(q->fitness < r->fitness) std::swap(q,r);
    a->M = p->M + (q->M - r->M) * factor;
}

float Population::stopWLOG(const std::vector<float>& v)
{
    const int param = 40;
    if (v.size() < 2*param) return 0.0f;
    /* jesli przez ostatnie 50 pokolen nie stalo sie nic ciekawszego niz podczas poprzednich 50 */
    float min1 = 1e10f, min2 = 1e10f;
    
    for (int i=v.size()-1; i>=(int)(v.size()-param); i--)
        min1 = std::min(min1, -v[i]);
    
    for (int i=v.size()-param-1; i >= 0 && i>=(int)(v.size()-2*param); i--)
        min2 = std::min(min2, -v[i]);
    
    // debug("  last 50: %f, not-so-last 50: %f\n", min1, min2);
    if (min1 == min2) return min1;
    if (min1 < min2) return 0.0f;
    if (min1 > min2) { debug("warning: unstable evolution convergence!\n"); return 0.0f; }
    return min1;
}
    
/* --- the so called main loop */
Agent Population::evolve()
{
    pop.resize(cfgPopulationSize);
    for(int i=0; i<(int)pop.size(); i++)
        makeRandom(&pop[i]);

    DisplaySlot best("best fit (POIs)");

    DisplaySlot bestIm("best fit (image)");
    { CairoImage *bestImCanvas = bestIm.getCanvas();
      bestImCanvas->resize(alien->raw->getWidth(), alien->raw->getHeight());
      bestIm.putCanvas(); }
    
    DisplaySlot diffIm("best fit (difference)");
    { CairoImage *diffImCanvas = diffIm.getCanvas();
      diffImCanvas->resize(alien->raw->getWidth(), alien->raw->getHeight());
      diffIm.putCanvas(); }

    diffIm.bind();
    std::vector<float> bestValues;
    
    Agent bestEver;
    bestEver.target = -1000000.0f;
    for(int gencnt=1;;gencnt++)
    {
        evaluate();

        {
            best.update(renderPOIs(known->transformPOIs(pop[0].M),
                                   alien->raw->getWidth(), alien->raw->getHeight()));
            diffIm.recaption("gen: %d, best fit: dist %.2f, diff %.2f, target %.2f fitness %f\nd=(%.0f,%.0f), s=(%.2f,%.2f), a=%.3f",
                             gencnt,
                             pop[0].distance, pop[0].difference, pop[0].target, pop[0].fitness,
                             pop[0].dx(),pop[0].dy(),pop[0].sx(),pop[0].sy(),pop[0].alfa()/M_PI*180.0);

            CairoImage *bestImCanvas = bestIm.getCanvas();
            Composite::transform(known->compimg, pop[0].M, bestImCanvas);
            bestIm.putCanvas();
            
            CairoImage *diffImCanvas = diffIm.getCanvas();
            Composite::difference(known->compimg, *alien->raw, pop[0].M, diffImCanvas);
            diffIm.putCanvas();

            if (gencnt % 20 == 0) debug("%d %f\n", gencnt, pop[0].target);
        }
        bestValues.push_back(pop[0].target);
        if (bestEver.target < pop[0].target)
            bestEver = pop[0];
        
        int survivors = cfgSurvivalRate * pop.size();
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
        
        float minstop = stopWLOG(bestValues);
        if (minstop != 0.0f || gencnt >= 300) break;
    }
    debug("there's no point going further. stopping\n");
    // while (true) ;
    return bestEver;
}

Population::Population(const Data *known, const Data *alien)
{
    this->known = known;
    this->alien = alien;
}

struct Result 
{
    static const int ILE = 5;
    int ile;
    Data* tab[ILE];
    float value[ILE];
};

inline static bool fooCompare(std::pair<Data*,Agent> p1, std::pair<Data*,Agent> p2) { return p1.second.target > p2.second.target; }
class Database
{
    std::vector<Data> datable;
    void init(const char* filename);
public:
    inline int size() const { return datable.size(); }
    inline Database(const char* filename) { init(filename); }
    Result query(const char* filename, const char* cath = NULL) /* taki nasz "main" */
    {
        Image alienImg = Image::readPGM(filename);
        Data alienDat = Data::build(filename);
        
        std::vector< std::pair<Data*,Agent> > similars;
        for (int i=0; i<size(); i++)
        {
            if (cath != NULL && strcmp(datable[i].cathegory,cath) != 0) continue;
            Agent A = Population(&datable[i], &alienDat).evolve();
            similars.push_back(std::make_pair(&datable[i], A));
        }
        
        sort(similars.begin(), similars.end(), fooCompare);
        Result ret;
        
        for (ret.ile=0; ret.ile<Result::ILE && ret.ile<similars.size(); ret.ile++)
            ret.tab[ret.ile] = similars[ret.ile].first,
            ret.value[ret.ile] = similars[ret.ile].second.target;
        
        return ret;
    }   
};

void Database::init(const char* filename)
{
    linereader lrd(filename);
    char cath[64];
    char path[256];
    char name[64];
    while (lrd.getline())
    {
        char *begin = lrd.buffer, *end = lrd.buffer+lrd.line_len;
        if (*begin == '#') continue;
        if (begin[0] == ':' && begin[1] == ':') 
        {
            begin ++; begin ++;
            char *put = cath;
            while (isspace(*begin)) begin ++;
            for (char *read = begin; read != end && !isspace(*read); read ++) *(put++) = *read;
            *put = '\0';
            /* changes current cathegory only */
            debug("now current cathegory is %s\n", cath);
        }
        else
        {
            char *read = begin;
            /* path */
            char *put = path;
            while (isspace(*read)) read ++;
            for(; read != end; read++)
                if (isspace(*read) && read[-1] != '\\') break; //file names may contain whitespaces, preceded by backslash
                else *(put++) = *read;
            *put = '\0';
            /* name */
            put = name;
            while (isspace(*read)) read ++;
            for (; read != end && *read != '\n'; read ++) *(put++) = *read;
            *put = '\0';
            
            if (strlen(name) == 0 || strlen(path) == 0) {
                debug("empty name or path\n");
                continue;
            }
            /* now initialize data */
            debug("bulding image from %s, its name is %s, cathegory %s\n", path, name, cath);
            
            datable.push_back(Data::build(path));
            strcpy(datable.back().name, name);
            strcpy(datable.back().cathegory, cath);
        }
    }
}


/* ------------------------------------------------------------------------ */

int main(int argc, char *argv[])
{
    srand(time(0));
    parse_config("evolution.cfg", cfgvars);
    spawn_worker_threads(cfgThreads); /* should detect no. of cpus available */
    
    gui_gtk_init(&argc, &argv); /* always before looking argc, argv */

    if (argc != 3) {
        fprintf(stderr, "usage: [input: alien] [cathegory]\n");
        return 1;
    }
    
    Database DTB("evolution.database");
    Result res = DTB.query(argv[1], argv[2]);
    
    for (int i=0; i<res.ile; i++)
        printf("%s, %s (%f)\n", res.tab[i]->name, res.tab[i]->cathegory, res.value[i]);
    
    return 0;
    
    Image knownImg = Image::readPGM(argv[1]),
          alienImg = Image::readPGM(argv[2]);
          
/*    Image diff = renderGaussianDifference(knownImg, alienImg, 2.0f, 2, false);
    diff.writePGM("foo.pgm");
    return 2; */
          
    Data knownDat = Data::build(argv[1]),
         alienDat = Data::build(argv[2]);

    DisplaySlot knownImgDS("known image"), alienImgDS("alien image"),
                knownPOIsDS("known POIs"), alienPOIsDS("alien POIs");

    knownImgDS.update(*knownDat.raw);
    alienImgDS.update(*alienDat.raw);
    knownPOIsDS.update(renderPOIs(knownDat.pois, knownImg.getWidth(), knownImg.getHeight()));
    alienPOIsDS.update(renderPOIs(alienDat.pois, alienImg.getWidth(), alienImg.getHeight()));

    Agent A = Population(&knownDat, &alienDat).evolve();
    
    return 0;
}

