#ifndef __UTIL_H__
#define __UTIL_H__

#include <cstdlib>
#include <cmath>
#include <ctime>
#include <cstring>
#include <stdexcept>
#include <pthread.h>

#define info(fmt, ...)  printf("   " fmt "\n", ## __VA_ARGS__)
#define okay(fmt, ...)  printf(" + " fmt "\n", ## __VA_ARGS__)
#define warn(fmt, ...)  printf("-- " fmt "\n", ## __VA_ARGS__)
#define fail(fmt, ...)  printf("!! " fmt "\n", ## __VA_ARGS__)
#define debug(fmt, ...) printf(".. " fmt "\n", ## __VA_ARGS__)

extern void (*progress)(float val);
void null_progress(float);
void text_progress(float val);

/* -------------------------------------------------------------------------- */

template <typename T> class Array2D
{
protected:
    int width, height;
    T *data;

    inline void _init() { width = height = 0; data = NULL; }
public:
    inline Array2D() { _init(); }
    inline Array2D(int w, int h) { _init(); resize(w,h); }
    inline Array2D(const Array2D<T> &im) { _init(); (*this) = im; }
    inline ~Array2D() { free(data); }
    
    inline T* operator[](int row) const { return data + row*width; }

    inline void resize(int w, int h)
    {
        if(width == w && height == h)
            return;
        width = w; height = h;
        data = (T *)realloc(data, width*height*sizeof(T));
        if(width && height && !data) throw std::bad_alloc();
    }

    inline Array2D<T>& operator=(const Array2D<T>& im)
    {
        resize(im.width, im.height);
        memcpy(data, im.data, width*height*sizeof(T));
        return *this;
    }
    
    inline int getWidth() const { return width; }
    inline int getHeight() const { return height; }
    inline bool inside(int x, int y) const { return x>=0 && y>=0 && x<width && y<height; }

    inline void fill(const T &v) {
        for(int i=0; i<width*height; i++)
            data[i] = v;
    }
};

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
    static float gaussian(float mean, float deviation) {
        /* Box-Muller transform ftw ;) */
        float p = positive(1), q = positive(1);
        float g = sqrtf(-2.0f * logf(p)) * cosf(2.0f*M_PI*q);
        return g*deviation + mean;
    }
};

/* -------------------------------------------------------------------------- */

struct Point
{
    float x,y;

    inline Point() { }
    inline Point(float x, float y) : x(x), y(y) { }
    inline Point(const Point &p) : x(p.x), y(p.y) { }

    inline Point operator+(const Point &p) const { return Point(x+p.x, y+p.y); }
    inline Point operator-(const Point &p) const { return Point(x-p.x, y-p.y); }
    inline Point operator*(float k) const { return Point(x*k, y*k); }
    inline float distsq() const { return x*x + y*y; }
    inline float dist() const { return sqrtf(distsq()); }
    /* distance used in evaluation */
    inline float disteval(float param = 0.0f) const { float a = dist(); return (param < 1e-3 || a < param) ? a : a*a*a / param*param; }

    inline bool operator<(const Point &p) const {
        return x != p.x ? x < p.x : y < p.y;
    }
};

/* -------------------------------------------------------------------------- */

struct FourFloats
{
    float xx,xy,yx,yy;
    inline FourFloats() {}
    inline FourFloats(float a, float b, float c, float d) {
        xx=a, xy=b, yx=c, yy=d;
    }
    inline bool inside(Point p) {
        return p.x >= xx && p.x <= xy && p.y >= yx && p.y <= yy;
    }
};


/* -------------------------------------------------------------------------- */

class Matrix
{
    /* the matrix:
     * | a b c |
     * | d e f |
     * | 0 0 1 |
     */
    float data[6];

public:
    inline float* operator[](int row) { return data+row*3; }
    inline const float* operator[](int row) const { return data+row*3; }

    inline Matrix()
    {
        Matrix &me = *this;
        me[0][0] = me[1][1] = 1;
        me[0][1] = me[0][2] = me[1][0] = me[1][2] = 0;
    }

    static inline Matrix translation(float dx, float dy)
    {
        Matrix ret;
        ret[0][0] = ret[1][1] = 1.0f;
        ret[0][2] = dx; ret[1][2] = dy;
        return ret;
    }
    static inline Matrix rotation(float alpha)
    {
        Matrix ret;
        ret[0][0] = ret[1][1] = cosf(alpha);
        ret[1][0] = -(ret[0][1] = sinf(alpha));
        return ret;
    }
    static inline Matrix scaling(float sx, float sy)
    {
        Matrix ret;
        ret[0][0] = sx; ret[1][1] = sy;
        return ret;
    }

    inline Matrix operator+(const Matrix &M) const
    {
        const Matrix &me = *this;
        Matrix ret;
        for (int i=0; i<2; i++)
            for (int j=0; j<3; j++)
                ret[i][j] = me[i][j]+M[i][j];
        return ret;
    }
    inline Matrix operator-(const Matrix &M) const
    {
        const Matrix &me = *this;
        Matrix ret;
        for (int i=0; i<2; i++)
            for (int j=0; j<3; j++)
                ret[i][j] = me[i][j]-M[i][j];
        return ret;
    }
    inline Matrix operator*(float k) const
    {
        const Matrix &me = *this;
        Matrix ret;
        for (int i=0; i<2; i++)
            for (int j=0; j<3; j++)
                ret[i][j] = me[i][j]*k;
        return ret;
    }
    inline Matrix operator/(float k) const
    {
        return (*this) * (1./k);
    }

    inline Point operator*(const Point &p) const
    {
        const Matrix &me = *this;
        return Point(me[0][0]*p.x + me[0][1]*p.y + me[0][2], 
                     me[1][0]*p.x + me[1][1]*p.y + me[1][2]);
    }

    inline Matrix operator*(const Matrix &M) const
    {
        const Matrix &me = *this;
        Matrix ret;

        ret[0][0] = me[0][0]*M[0][0] + me[0][1]*M[1][0];
        ret[0][1] = me[0][0]*M[0][1] + me[0][1]*M[1][1];
        ret[0][2] = me[0][0]*M[0][2] + me[0][1]*M[1][2] + me[0][2];
        
        ret[1][0] = me[1][0]*M[0][0] + me[1][1]*M[1][0];
        ret[1][1] = me[1][0]*M[0][1] + me[1][1]*M[1][1];
        ret[1][2] = me[1][0]*M[0][2] + me[1][1]*M[1][2] + me[1][2];

        return ret;
    }

    inline Matrix inverse() const /* hell, yeah! */ /* but.. why? */
    {
        const Matrix &M = *this;
        Matrix R;

        R[0][0] = M[1][1];
        R[1][0] = -M[1][0];

        R[0][1] = -M[0][1];
        R[1][1] = M[0][0];

        R[0][2] = M[0][1]*M[1][2] - M[0][2]*M[1][1];
        R[1][2] = M[0][2]*M[1][0] - M[0][0]*M[1][2];

        return R / (M[1][1]*M[0][0] - M[1][0]*M[0][1]);
    }
};

/* -------------------------------------------------------------------------- */

class Timer {
    struct timespec ts;
    bool isrunning;
    /* when running, delay is a total pause time
     * when paused, delay is total pause time - last pause time */
    float delay;
    int clock;
    /* real time from the start till now */
    float gettime();
public:
    inline Timer(int clock = CLOCK_THREAD_CPUTIME_ID) : isrunning(false), clock(clock) { }
    void start();
    void pause();
    void resume();
    /* gets real time from start till now - total pause time */
    float end();
};

/* -------------------------------------------------------------------------- */

class Completion
{
    pthread_cond_t cond;
    pthread_mutex_t mutex;
    int counter;

public:
    Completion();
    ~Completion();

    void completed();
    void queued();
    void wait();
};

class AsyncJob
{
    friend class AsyncQueue;
    AsyncJob *next;

public:
    Completion *completion;

    virtual ~AsyncJob();
    virtual void run() = 0;
};

class AsyncQueue
{
    pthread_mutex_t mutex;
    pthread_cond_t cond;

    AsyncJob *first, **last;

public:
    AsyncQueue();
    ~AsyncQueue();

    void queue(AsyncJob *job);
    AsyncJob *get(void);
};

class WorkerThread
{
    pthread_t thread;
    AsyncQueue *queue;
    
public:
    WorkerThread(AsyncQueue *queue);
    void start();
    void run();
};

extern AsyncQueue *aq;
void spawn_worker_threads(int n);

#endif

