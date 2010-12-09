#include <cmath>
#include <vector>
#include <cstdlib>
#include <unistd.h>
#include <algorithm>

#include "poi.h"
#include "evolution.h"
#include "workers.h"
#include "gui.h"
#include "image.h"

#define debug(fmt, ...) fprintf(stderr, fmt, ## __VA_ARGS__)

inline float randomFloat(float maxv = 1.0f) //jednostajnie z przedziału [0, 1), 
{ 
    return maxv*(float)(rand()%1000000)/1000000.0f+(float)(rand()%1000000)/1000000000000.0f; 
}
inline float randomSign() { return (rand()&1)? -1.0f : 1.0f; }
inline float randomSigned(float maxv = 1.0f) { return randomFloat(maxv)*randomSign(); }

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
    
    inline static Matrix33 Rtranslation(float maxvx, float maxvy) { return translation(randomSigned(maxvx), randomSigned(maxvy)); }
    inline static Matrix33 Rrotation(float maxv) { return rotation(randomSigned(maxv)); }
    inline static Matrix33 Rscaling(float maxsx, float maxsy);
    
    inline POI operator*(const POI &p) const;
    inline Matrix33 operator*(const Matrix33 &M) const;
};

Matrix33::Matrix33()
{
    Matrix33 &me = *this;
    me[0][0] = me[0][1] = me[0][2] = me[1][0] = me[1][1] = me[1][2] = me[2][0] = me[2][1] = me[2][2] = 0.0f;
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

Matrix33 Matrix33::Rscaling(float maxsx, float maxsy)
{
    Matrix33 ret;
    ret[0][0] = exp(randomSigned(maxsx)); ret[1][1] = exp(randomSigned(maxsy));
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


/* instance of the image, that is an image (is it really necessary?) itself 
 * with a lot of additional things we managed to compute (POIs, mostly) */
class ImageInstance 
{
public:
    const Image *rawImg;
    std::vector<POI> poiVec;
    /* jakieś jeszcze rzeczy, typu mapka kolorów */
      
    inline ImageInstance(const ImageInstance &inst) {
        rawImg = inst.rawImg; //wskaznik ten sam, bo i obrazek ten sam
        poiVec = inst.poiVec;
    }
        
    inline ImageInstance(const Image& img, bool evaluate) {
        rawImg = &img;
        std::vector<float> scales(3,0); scales[0] = 1; scales[1] = 4; scales[2] = 7;
        if (evaluate) poiVec = findPOIs(evaluateImage(img,20,scales), 7, 40.0, 50); // parametry dobrane heurystycznie
    }
    
    float distance(const ImageInstance& inst);
    
    inline void toImage(Image* dst) { renderPOIs(poiVec, dst, 1, 100000); }
};

float ImageInstance::distance(const ImageInstance& inst)
{
    /* dla każdego punktu wektora jednego obrazka szukamy najbliższego mu w drugim wektorze */
    /* na razie metryka: suma odległości */
    float sum = 0.0f;
    for (int i=0; i<(int)poiVec.size(); i++)
    {
        float mindist = 100000.0f;
        for (int j=0; j<(int)inst.poiVec.size(); j++)
            mindist = std::min(mindist, dist(poiVec[i], inst.poiVec[j]));
        sum += mindist;
    }
    return sum;
}

/* ------------------------------------------------------------------------ */

class Individual 
{
public:
    float value;

    inline Individual() : value(-1.0f) { }
    virtual Matrix33 toMatrix() const = 0;
    
    inline bool operator<(const Individual& I) const { return value < I.value; }
};

class Agent : public Individual
{public:
    Matrix33 M;
    float W, H; 
    /* musimy znac wymiary obrazka zeby wiedziec jakie jest sensowne przesuniecie
     * to sa wymiary obrazka DOCELOWEGO, a nie przetwarzanego !! */
    
    inline float dx() { return M[0][2]; }
    inline float dy() { return M[1][2]; }
    inline float scx() { return sqrtf(M[0][0]*M[0][0]+M[0][1]*M[0][1]); }
    inline float scy() { return sqrtf(M[1][0]*M[1][0]+M[1][1]*M[1][1]); }
    inline float alfa() { float sx = scx(); return atan2(M[0][1]/sx, M[0][0]/sx); }
        
    inline Agent(int w, int h, bool random = true); //random init
    Matrix33 toMatrix() const { return M; }

    std::vector<POI> applyToPOIs(const std::vector<POI> &v);
    ImageInstance apply(const ImageInstance& inst);
};

Agent::Agent(int w, int h, bool random)
{
    W = w; H = h;
    if (random) {
        M[0][2] = randomFloat(W);
        M[1][2] = randomFloat(H);
        float scx = randomSigned(5.0f),
              scy = randomSigned(5.0f),
              alfa = randomFloat(2.0f*(float)M_PI);
        M[0][0] = cosf(alfa)*scx;
        M[0][1] = sinf(alfa)*scx;
        M[1][0] = -sinf(alfa)*scy;
        M[1][1] = cosf(alfa)*scy;
        M[2][2] = 1.0f;
    }
    else {
        M[0][0] = M[1][1] = M[2][2] = 1.0f;
    }
}

std::vector<POI> Agent::applyToPOIs(const std::vector<POI> &v)
{
    std::vector<POI> ret(v.size());
    for (int i=0; i<(int)v.size(); i++) 
        ret[i] = M*v[i];
    return ret;
}

ImageInstance Agent::apply(const ImageInstance& inst)
{
    ImageInstance ret(*inst.rawImg, false);
    ret.poiVec = applyToPOIs(inst.poiVec);
    return ret;
}

/* ------------------------------------------------------------------------ */

class Evolution /* main class */
{
    int N;
    float W, H; //niestety, chyba muszę pamiętać te wartości. Rozmiary obrazka docelowego
    const Image *orig, *maybe;
    ImageInstance *origInst, *maybeInst;
    std::vector<Agent> pop;
public:
    Evolution(Image* imorig, Image* immay, int n);
    inline static float takerandom(float a, float b) { return (randomSign()>0.0 ? a : b); }
    inline void randomInit() { for (int i=0; i<N; i++) pop.push_back(Agent(W, H, true)); }
    inline static void mutate(float &value, float dv, float prob) { 
        value += (randomFloat()<prob? randomFloat()*dv*2.0-dv : 0); 
    }
    inline void mutate(Agent& A, float prob, float change)
    {
        const float dsc = 0.01f*change;
        const float dmove = 1.0f*change;
        const float dalfa = 0.05f*change;
        int which = rand()%3;
        switch (which) {
            case 0: A.M = A.M*Matrix33::Rtranslation(dmove*W, dmove*H); break;
            case 1: A.M = A.M*Matrix33::Rscaling(dsc, dsc); break;
            case 2: A.M = A.M*Matrix33::Rrotation(dalfa); break;
        }
    }
    
    inline float evaluate(Agent& A) { return A.value = 1.0f/origInst->distance(A.apply(*maybeInst)); }
    inline void evaluate() { for (int i=0; i<N; i++) evaluate(pop[i]); }
    Agent mainLoop(int maxiter = 1000, DisplaySlot *bestDS = NULL, DisplaySlot *origDS = NULL, DisplaySlot *maybeDS = NULL)
    {
        float change = 10.0f;
        randomInit();
        
        Image origPOIimg(W, H);
        Image maybePOIimg(maybe->getWidth(), maybe->getHeight());
        Image tmpImg(W, H);
        
        origInst->toImage(&origPOIimg);
        maybeInst->toImage(&maybePOIimg);
        
        if (origDS != NULL) origDS->update(origPOIimg);
        if (maybeDS != NULL) maybeDS->update(maybePOIimg);
        
        for (int it=0; it<maxiter; it++)
        {
            for (int i=0; i<N/2; i++)
                pop[i+N/2] = pop[i];
            
            for (int i=0; i<N; i++) mutate(pop[i], 0.5, change);
            change *= 0.998;
            evaluate();
            std::sort(pop.rbegin(), pop.rend());
            if (bestDS != NULL) {
                pop[0].apply(*maybeInst).toImage(&tmpImg);
                bestDS->update(tmpImg);
                bestDS->recaption("distance = %.1f. angle=%.2f, dx=%.1f, dy=%.1f, scx=%.1f, scy=%.1f", 
                               1.0f/pop[0].value, pop[0].alfa(), pop[0].dx(), pop[0].dy(), pop[0].scx(), pop[0].scy());
            }
        }
        return pop[0];
    }
};

Evolution::Evolution(Image* imorig, Image* immay, int n) 
{
    N = n;
    orig = imorig; 
    maybe = immay;
    W = orig->getWidth(); H = orig->getHeight();
    origInst = new ImageInstance(*orig, true);
    maybeInst = new ImageInstance(*maybe, true);
}

/* ------------------------------------------------------------------------ */

int main(int argc, char *argv[])
{
    srand(time(0));
    if (argc < 3/*4*/) {
        fprintf(stderr, "usage: [input: original] [input: pretending]\n");// [output: match\n");
        return 1;
    }
    /* będziemy zmieniac obrazek "pretending" tak, aby otrzymać original */
    gui_gtk_init(&argc, &argv);
    spawn_worker_threads(2);
    
    Image origImg = Image::readPGM(argv[1]);
    Image pretendImg = Image::readPGM(argv[2]);
    
    int H = origImg.getHeight(), W = origImg.getWidth();
    Image tmpImg(W, H);
    Image bestImg(W, H);
    
    DisplaySlot origDS, maybeDS, bestmatchDS;
    origDS.rename("original image");
    origDS.recaption("");
    maybeDS.rename("tested image");
    maybeDS.recaption("");
    bestmatchDS.rename("best match between images");
    bestmatchDS.recaption("");
    origDS.update(origImg);
    maybeDS.update(pretendImg);
    
    debug("okay, start evolving\n");
    
    Evolution EVO(&origImg, &pretendImg, 1000);
    EVO.mainLoop(1000000, &bestmatchDS, &origDS, &maybeDS);
    
    return 0;
}
        



