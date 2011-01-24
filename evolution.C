#include <cstdlib>
#include <cstdio>
#include <cmath>
#include <cassert>
#include <ctime>
#include <ctype.h>
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

#define info(fmt, ...)  printf("   " fmt "\n", ## __VA_ARGS__)
#define okay(fmt, ...)  printf(" + " fmt "\n", ## __VA_ARGS__)
#define warn(fmt, ...)  printf("-- " fmt "\n", ## __VA_ARGS__)
#define fail(fmt, ...)  printf("!! " fmt "\n", ## __VA_ARGS__)
#define debug(fmt, ...) printf(".. " fmt "\n", ## __VA_ARGS__)

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
static int cfgStopCondParam, cfgMaxGeneration;
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
    { "stopCondParam",  config_var::INT,       &cfgStopCondParam },
    { "maxGeneration",  config_var::INT,       &cfgMaxGeneration },
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

    std::vector<POI> transformPOIs(const Matrix &M) const;
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

std::vector<POI> Data::transformPOIs(const Matrix &M) const
{
    std::vector<POI> xfmd(pois.size());
    for(int i=0; i<(int)pois.size(); i++)
        xfmd[i] = M * pois[i];
    return xfmd;
}

/* ------------------------------------------------------------------------ */

class ImageDisplaySlot : public DisplaySlot
{
public:
    const Data *data;
    rgba color;

    inline ImageDisplaySlot(const char *name, const Data *data = NULL, rgba color = rgba(0,0,0,0))
        : DisplaySlot(name), data(data), color(color) { }
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
    const Data *known; rgba knownColor;
    const Data *alien; rgba alienColor;
    Matrix m;

    inline FitDisplaySlot(const char *name,
                          const Data *known = NULL, rgba knownColor = rgba(0,0,0,0),
                          const Data *alien = NULL, rgba alienColor = rgba(0,0,0,0),
                          const Matrix &m = Matrix()) :
        DisplaySlot(name), known(known), knownColor(knownColor), alien(alien), alienColor(alienColor), m(m) { }

    virtual ~FitDisplaySlot() { }

    virtual void draw()
    {
        if(!known || !alien)
            return;

/*        struct timespec tstart, tend;
        clock_gettime(CLOCK_THREAD_CPUTIME_ID, &tstart); */

        resize(alien->raw.getWidth(), alien->raw.getHeight());
        drawImage(alien->caimg);
        drawDifference(known->caimg, m);
        
        std::vector<Point> ps(alien->pois.size());
        for(int i=0; i<(int)alien->pois.size(); i++)
            ps[i] = alien->pois[i];
        drawDots(ps,6, alienColor);

        ps.resize(known->pois.size());
        for(int i=0; i<(int)known->pois.size(); i++)
            ps[i] = m * known->pois[i];
        drawDots(ps,6, knownColor);
        
/*      clock_gettime(CLOCK_THREAD_CPUTIME_ID, &tend);
        long long p = 1000000000LL*tstart.tv_sec + tstart.tv_nsec,
                  q = 1000000000LL*tend.tv_sec   + tend.tv_nsec,
                  r = q-p;
        debug("drawing took %d.%06d ms", (int)(r/1000000), (int)(r%1000000)); */
    }
};

/* those are created statically, however they will only initialize
 * themselves upon first action, which must occur after gtk_gui_init. */

static ImageDisplaySlot knownDS("known image", NULL, rgba(0.1,1,0.1,0.5)),
                        alienDS("alien image", NULL, rgba(1,0.3,0.1,0.5));
static FitDisplaySlot   bestDS("best fit", NULL, rgba(0.1,1,0.1,0.5), NULL, rgba(1,0.3,0.1,0.5));

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

    /* one needs to know when to stop! */
    inline bool terminationCondition() const;

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
            best = std::min(best, (a[i]-b[j]).dist());
        sum += best;
    }
    return sum;
}
void Population::EvaluationJob::run()
{
    const Data *alien = uplink->alien, *known = uplink->known;

    std::vector<POI> xformed = known->transformPOIs(agent->M);
    agent->target = -(matchPOIs(xformed, alien->pois) + matchPOIs(alien->pois, xformed)) /
                     (alien->pois.size() + known->pois.size());
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

    warn("roulette selection failed, residual %e", y);
    return 0;
}

/* --- mutations */
void Population::makeRandom(Agent *a)
{
    a->M = Matrix();
    a->translate(
            Random::real(cfgTranslateInit) * known->raw.getWidth(),
            Random::real(cfgTranslateInit) * known->raw.getHeight());
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
        float rangle = Random::real(6.28f),
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

bool Population::terminationCondition() const
{
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
    pop.resize(cfgPopulationSize);
    for(int i=0; i<(int)pop.size(); i++)
        makeRandom(&pop[i]);

    knownDS.lock(); knownDS.data = known; knownDS.unlock();
    alienDS.lock(); alienDS.data = alien; alienDS.unlock();
    bestDS.lock(); bestDS.known = known; bestDS.alien = alien; bestDS.unlock();

    Agent bestEver;
    bestEver.target = -1000000.0f;

    for(int gencnt=1;;gencnt++)
    {
        evaluate();

		gui_status("gen: %d, best fit: target %.2f fitness %f  |  d=(%.0f,%.0f), s=(%.2f,%.2f), a=%.3f",
			       gencnt,
				   pop[0].target, pop[0].fitness,
				   pop[0].dx(),pop[0].dy(),pop[0].sx(),pop[0].sy(),pop[0].alfa()/M_PI*180.0);

        bestDS.lock();
        bestDS.m = pop[0].M;
        bestDS.unlock();

        bestScores.push_back(pop[0].target);
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
        
        if (terminationCondition() || gencnt >= cfgMaxGeneration) break;
    }
    
    knownDS.lock(); knownDS.data = NULL; knownDS.unlock();
    alienDS.lock(); alienDS.data = NULL; alienDS.unlock();
    bestDS.lock(); bestDS.known = bestDS.alien = NULL; bestDS.unlock();

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
    Data alien = Data::build(argv[1]);

    /* examine all known images */
    std::vector<std::pair<float, const char *> > results;    
    for(int i=0; i<(int)knownPaths.size(); i++)
    {
        const char *knownPath = knownPaths[i].c_str();
        okay("processing '%s'", knownPath);
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
