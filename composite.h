#ifndef __COMPOSITE_H__
#define __COMPOSITE_H__

#include <cstring>

#include "workers.h"
#include "image.h"

typedef struct _cairo_surface cairo_surface_t;
typedef struct _cairo cairo_t;

class CairoImage
{
    int width, height, stride;
    void *data;
    cairo_surface_t *cr_surface;

    inline void _init() { width = height = 0; data = NULL; cr_surface = NULL; }
public:
    inline CairoImage() { _init(); }
    inline CairoImage(int w, int h) { _init(); resize(w,h); }
    inline CairoImage(const CairoImage &im) { _init(); (*this)=im; }
    inline CairoImage(const Image &im, bool transparency = false) { _init(); fromImage(im, transparency); }

    ~CairoImage();
    
    void resize(int w, int h);
    
    inline CairoImage& operator=(const CairoImage &im) {
        resize(im.width, im.height);
        memcpy(data, im.data, height*stride);
        return *this;
    }
    
    void fromImage(const Image &src, bool transparency);
    void toImage(Image *dst);

    inline uint32_t* operator[](int row) const { return (uint32_t *)((char *)data + row*stride); }

    inline int getWidth() const { return width; }
    inline int getHeight() const { return height; }
    inline cairo_surface_t *getCairoSurface() const { return cr_surface; }
    inline bool inside(int x, int y) const { return x>=0 && y>=0 && x<width && y<height; }
};

class Composite
{
    static void do_transform(const CairoImage &src, const Matrix33 &M, CairoImage *dst);
public:
    static void prepare(const Image &src, CairoImage *dst);
    static void transform(const CairoImage &src, const Matrix33 &M, CairoImage *dst);
    static void difference(const CairoImage &known, const Image &alien, const Matrix33 &M, CairoImage *dst);
    static float difference(const CairoImage &known, const Image &alien, const Matrix33 &M);
};

#endif

