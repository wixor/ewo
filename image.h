#ifndef __IMAGE_H__
#define __IMAGE_H__

#include <cstdlib>
#include <cstring>
#include <cmath>
#include <stdint.h>
#include <stdexcept>

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
        if(!data) throw std::bad_alloc();
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
};

class Image : public Array2D<uint8_t>
{
public:
    inline Image() { }
    inline Image(int w, int h) : Array2D<uint8_t>(w, h) {}
    inline Image(const Image& im) : Array2D<uint8_t>(im) {}
        
    static Image readPGM(FILE *file);
    static Image readPGM(const char *filename);
    void writePGM(FILE* file) const;
    void writePGM(const char* filename) const;

    inline void fill(uint8_t with) { memset(data, with, width*height); }
    void drawRect(int x0, int x1, int y0, int y1, uint8_t val);

    uint32_t checksum() const;
};

/* ------------------------------------------------------------------------ */

struct POI {
    int x, y;
    float val;

    inline POI() { }
    inline POI(int x, int y, float v) : x(x), y(y), val(v) { }
    inline bool operator<(const POI &p) const {
        return val != p.val ? val > p.val :
                 x != p.x   ? x   < p.x :
                              y   < p.y;
    }

    static inline float dist(const POI &a, const POI &b) {
        return sqrtf((a.x-b.x)*(a.x-b.x) + (a.y-b.y)*(a.y-b.y));
    }
};

/* ------------------------------------------------------------------------ */

class Matrix33
{
    float data[9];

public:
    inline float* operator[](int row) { return data+row*3; }
    inline const float* operator[](int row) const { return data+row*3; }

    inline Matrix33()
    {
        Matrix33 &me = *this;
        me[0][0] = me[1][1] = me[2][2] = 1;
        me[0][1] = me[0][2] = me[1][0] = me[1][2] = me[2][0] = me[2][1] = 0;
    }

    static inline Matrix33 translation(float dx, float dy)
    {
        Matrix33 ret;
        ret[0][0] = ret[1][1] = ret[2][2] = 1.0f;
        ret[0][2] = dx; ret[1][2] = dy;
        return ret;
    }
    static inline Matrix33 rotation(float alpha)
    {
        Matrix33 ret;
        ret[0][0] = ret[1][1] = cosf(alpha);
        ret[1][0] = -(ret[0][1] = sinf(alpha));
        ret[2][2] = 1.0f;
        return ret;
    }
    static inline Matrix33 scaling(float sx, float sy)
    {
        Matrix33 ret;
        ret[0][0] = sx; ret[1][1] = sy;
        ret[2][2] = 1.0f;
        return ret;
    }

    inline Matrix33 operator+(const Matrix33 &M) const
    {
        const Matrix33 &me = *this;
        Matrix33 ret;
        for (int i=0; i<3; i++)
            for (int j=0; j<3; j++)
                ret[i][j] = me[i][j]+M[i][j];
        return ret;
    }
    inline Matrix33 operator-(const Matrix33 &M) const
    {
        const Matrix33 &me = *this;
        Matrix33 ret;
        for (int i=0; i<3; i++)
            for (int j=0; j<3; j++)
                ret[i][j] = me[i][j]-M[i][j];
        return ret;
    }
    inline Matrix33 operator*(float k) const
    {
        const Matrix33 &me = *this;
        Matrix33 ret;
        for (int i=0; i<3; i++)
            for (int j=0; j<3; j++)
                ret[i][j] = me[i][j]*k;
        return ret;
    }

    inline POI operator*(const POI &p) const
    {
        const Matrix33 &me = *this;
        return POI(me[0][0]*p.x + me[0][1]*p.y + me[0][2], 
                   me[1][0]*p.x + me[1][1]*p.y + me[1][2],
                   p.val);
    }

    inline Matrix33 operator*(const Matrix33 &M) const
    {
        const Matrix33 &me = *this;
        Matrix33 ret;
        for (int i=0; i<3; i++)
            for (int j=0; j<3; j++)
                ret[i][j] = me[i][0]*M[0][j] + me[i][1]*M[1][j] + me[i][2]*M[2][j];
        return ret;
    }

    inline Matrix33 inverse() const /* hell, yeah! */ /* but.. why? */
    {
        const Matrix33 &M = *this;
        Matrix33 R;

        R[0][0] = M[1][1]*M[2][2] - M[1][2]*M[2][1];
        R[1][0] = M[1][2]*M[2][0] - M[1][0]*M[2][2];
        R[2][0] = M[1][0]*M[2][1] - M[1][1]*M[2][0];

        R[0][1] = M[0][2]*M[2][1] - M[0][1]*M[2][2];
        R[1][1] = M[0][0]*M[2][2] - M[0][2]*M[2][0];
        R[2][1] = M[0][1]*M[2][0] - M[0][0]*M[2][1];

        R[0][2] = M[0][1]*M[1][2] - M[0][2]*M[1][1];
        R[1][2] = M[0][2]*M[1][0] - M[0][0]*M[1][2];
        R[2][2] = M[0][0]*M[1][1] - M[0][1]*M[1][0];

        return R * (1./(R[0][0]*M[0][0] + R[1][0]*M[0][1] + R[2][0]*M[0][2]));
    }
};


#endif
