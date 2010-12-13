#ifndef __IMAGE_H__
#define __IMAGE_H__

#include <cstdlib>
#include <cstring>
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
    inline Image(const Image& im) : Array2D<uint8_t>(im) {}
    inline Image(int w, int h) : Array2D<uint8_t>(w, h) {}
    
    static Image readPGM(FILE *file);
    static Image readPGM(const char *filename);
    void writePGM(FILE* file) const;
    void writePGM(const char* filename) const;

    inline void fill(uint8_t with) { memset(data, with, width*height); }
    void drawRect(int x0, int x1, int y0, int y1, uint8_t val);

    uint32_t checksum() const;
};

class CairoImage
{
    int width, height, stride;
    void *data;
    void *cr_surface;

    inline void _init() { width = height = 0; cr_surface = data = NULL; }
public:
    inline CairoImage() { _init(); }
    inline CairoImage(int w, int h) { _init(); resize(w,h); }
    inline CairoImage(const CairoImage &im) { _init(); (*this)=im; }
    ~CairoImage();
    
    inline CairoImage& operator=(const CairoImage &im) {
        resize(im.width, im.height);
        memcpy(data, im.data, height*stride);
        return *this;
    }

    void setFromImage(const Image &im, bool transparency = false);

    void resize(int w, int h);

    inline uint32_t* operator[](int row) const { return (uint32_t *)((char *)data + row*stride); }

    inline int getWidth() const { return width; }
    inline int getHeight() const { return height; }
    inline void *getCairoSurface() const { return cr_surface; }
    inline bool inside(int x, int y) const { return x>=0 && y>=0 && x<width && y<height; }
};

#endif
