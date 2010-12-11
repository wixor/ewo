#include <cstdlib>
#include <cmath>
#include <cassert>
#include <ctype.h>
#include <unistd.h>
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
static int cfgPOICount;
static int cfgPopulationSize;
static float cfgSurvivalRate;
static float cfgTranslateInit, cfgRotateInit, cfgScaleInit;
static float cfgTranslateProp, cfgRotateProp, cfgScaleProp;
static float cfgTranslateDev, cfgRotateDev, cfgScaleDev;
static float cfgOriginDev;

static struct config_var cfgvars[] = {
    { "threads",        config_var::INT,       &cfgThreads },
    /* poi detection */
    { "poiSteps",       config_var::INT,       &cfgPOISteps },
    { "poiScales",      config_var::CALLBACK,  (void *)&parsePOIScales },
    { "poiCount",       config_var::INT,       &cfgPOICount },
    { "poiThreshold",   config_var::FLOAT,     &cfgPOIThreshold },
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
    { "translateDev",   config_var::FLOAT,     &cfgTranslateDev },
    { "rotateDev",      config_var::FLOAT,     &cfgRotateDev },
    { "scaleDev",       config_var::FLOAT,     &cfgScaleDev },
    /* standard deviation of rotation origin from the median point */
    { "originDev",      config_var::FLOAT,     &cfgOriginDev },
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

class Matrix33
{
    float data[9];

public:
    inline Matrix33();
    virtual ~Matrix33();
    
    inline float* operator[](int row) { return data+row*3; }
    inline const float* operator[](int row) const { return data+row*3; }

    inline static Matrix33 translation(float dx, float dy);
    inline static Matrix33 rotation(float alpha);
    inline static Matrix33 scaling(float sx, float sy);
    
    /* multiply by vector and matrix */
    inline POI operator*(const POI &p) const;
    inline Matrix33 operator*(const Matrix33 &M) const;
};

Matrix33::Matrix33()
{
    Matrix33 &me = *this;
    me[0][0] = me[1][1] = me[2][2] = 1;
    me[0][1] = me[0][2] = me[1][0] = me[1][2] = me[2][0] = me[2][1] = 0;
}

Matrix33::~Matrix33()
{
    /* empty */
}

Matrix33 Matrix33::translation(float dx, float dy)
{
    Matrix33 ret;
    ret[0][0] = ret[1][1] = ret[2][2] = 1.0f;
    ret[0][2] = dx; ret[1][2] = dy;
    return ret;
}
Matrix33 Matrix33::rotation(float alpha)
{
    Matrix33 ret;
    ret[0][0] = ret[1][1] = cosf(alpha);
    ret[1][0] = -(ret[0][1] = sinf(alpha));
    ret[2][2] = 1.0f;
    return ret;
}
Matrix33 Matrix33::scaling(float sx, float sy)
{
    Matrix33 ret;
    ret[0][0] = sx; ret[1][1] = sy;
    ret[2][2] = 1.0f;
    return ret;
}

POI Matrix33::operator*(const POI &p) const
{
    const Matrix33 &me = *this;
    return POI(me[0][0]*p.x + me[0][1]*p.y + me[0][2], 
               me[1][0]*p.x + me[1][1]*p.y + me[1][2],
               p.val);
}

Matrix33 Matrix33::operator*(const Matrix33 &M) const
{
    const Matrix33 &me = *this;
    Matrix33 ret;
    for (int i=0; i<3; i++)
        for (int j=0; j<3; j++)
            ret[i][j] = me[i][0]*M[0][j] + me[i][1]*M[1][j] + me[i][2]*M[2][j];
    return ret;
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
    Image *raw;
    std::vector<POI> pois;
    int originX, originY; /* median of POIs */
    /* + more to come (handle for opengl images compositing) */

    static Data build(const char *filename);

    std::vector<POI> transformPOIs(const Matrix33 &M) const;
};

Data Data::build(const char *filename)
{
    Data ret;
    ret.raw = new Image(0,0);
    (*ret.raw) = Image::readPGM(filename);

    if(!ret.readCached(filename)) {
        ret.findPOIs();
        ret.findOrigin();
        ret.writeCache(filename);
    }

    return ret;
}

void Data::findPOIs()
{
    Array2D<float> eval = ::evaluateImage(*raw, cfgPOISteps, cfgPOIScales);
    pois = ::findPOIs(eval, cfgPOIThreshold, cfgPOICount);
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
        int from, to;
        virtual ~EvaluationJob();
        virtual void run();
    };
    void evaluate();

    /* roulette selection */
    int roulette(int n);

    /* mutations and matings */
    void makeRandom(Agent *a);
    void mutation(Agent *a);

public:
    Population(const Data *known, const Data *alien);
    void evolve();
};

/* --- population evaluation */
Population::EvaluationJob::~EvaluationJob() {
    /* empty */
}
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
    const Data *known = uplink->known,
               *alien = uplink->alien;

    for(int i=from; i<to; i++)
    {
        Agent *a = &uplink->pop[i];
        std::vector<POI> xformed = known->transformPOIs(a->M);
        a->distance = -matchPOIs(xformed, alien->pois)
                      -matchPOIs(alien->pois, xformed);
    }
}
void Population::evaluate()
{
    const int jobSlice = 200;
    int nJobs = (pop.size() + jobSlice - 1) / jobSlice;
    EvaluationJob jobs[nJobs];
    Completion c;
    
    for(int i=0; i<nJobs; i++) {
        jobs[i].completion = &c;
        jobs[i].from = jobSlice*i;
        jobs[i].to = std::min(jobSlice*(i+1), (int)pop.size());
        jobs[i].uplink = this;
        aq->queue(&jobs[i]);
    }

    c.wait();

    float minDistance = 1e+30;
    for(int i=0; i<(int)pop.size(); i++)
        minDistance = std::min(minDistance, pop[i].distance);
    float denominator = 0;
    for(int i=0; i<(int)pop.size(); i++)
        denominator += pop[i].distance - minDistance;
    assert(denominator > 1e-5);
    for(int i=0; i<(int)pop.size(); i++)
        pop[i].fitness = (pop[i].distance - minDistance) / denominator;

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

    fprintf(stderr, "WARNING: roulette selection failed, residual %e", y);
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
}

/* --- the so called main loop */
void Population::evolve()
{
    pop.resize(cfgPopulationSize);
    for(int i=0; i<(int)pop.size(); i++)
        makeRandom(&pop[i]);

    DisplaySlot best("best fit");
    best.bind();

    for(;;)
    {
        evaluate();

        best.update(renderPOIs(known->transformPOIs(pop[0].M),
                               alien->raw->getWidth(), alien->raw->getHeight()));
        best.recaption("best fit: dist %.2f, fitness %f\nd=(%.0f,%.0f), s=(%.2f,%.2f), a=%.3f",
                       pop[0].distance, pop[0].fitness,
                       pop[0].dx(),pop[0].dy(),pop[0].sx(),pop[0].sy(),pop[0].alfa()/M_PI*180.0);


        int survivors = cfgSurvivalRate * pop.size();
        for(int i=survivors; i<(int)pop.size(); i++)
            pop[i] = pop[roulette(survivors)];

        for(int i=0; i<(int)pop.size(); i++)
            mutation(&pop[i]);
    }
}

Population::Population(const Data *known, const Data *alien)
{
    this->known = known;
    this->alien = alien;
}

/* ------------------------------------------------------------------------ */

int main(int argc, char *argv[])
{
    srand(time(0));
    parse_config("evolution.cfg", cfgvars);
    spawn_worker_threads(cfgThreads); /* should detect no. of cpus available */
    gui_gtk_init(&argc, &argv); /* always before looking argc, argv */

    if (argc != 3) {
        fprintf(stderr, "usage: [input: known] [input: alien]\n");
        return 1;
    }
    
    Image knownImg = Image::readPGM(argv[1]),
          alienImg = Image::readPGM(argv[2]);

    Data knownDat = Data::build(argv[1]),
         alienDat = Data::build(argv[2]);

    DisplaySlot knownImgDS("known image"), alienImgDS("alien image"),
                knownPOIsDS("known POIs"), alienPOIsDS("alien POIs");

    knownImgDS.update(*knownDat.raw);
    alienImgDS.update(*alienDat.raw);
    knownPOIsDS.update(renderPOIs(knownDat.pois, knownImg.getWidth(), knownImg.getHeight()));
    alienPOIsDS.update(renderPOIs(alienDat.pois, alienImg.getWidth(), alienImg.getHeight()));

    Population(&knownDat, &alienDat).evolve();
    
    return 0;
}

