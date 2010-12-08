#ifndef __IMAGE_H__
#define __IMAGE_H__

#include <stdint.h>
#include <cstring>
#include <cstdio>

template <typename T> class Array2D
{
protected:
    int width, height;
    T *data;
public:
    inline Array2D(int w, int h)
    {
        width = w;
        height = h;
        data = new T[width*height];
    }
    inline Array2D(const Array2D<T> &im)
    {
        width = im.width;
        height = im.height;
        data = new T[width*height];
        memcpy(data, im.data, width*height*sizeof(T));
    }
    virtual ~Array2D() {
        delete data;
    }
    
    inline T* operator[](int row) { return data+row*width; }
    inline const T* operator[](int row) const { return data+row*width; }

    inline void operator=(const Array2D<T>& im)
    {
        if(width != im.width || height != im.height)
        {
            width = im.width;
            height = im.height;

            delete data;
            data = new T[width*height];
        }
        memcpy(data, im.data, width*height*sizeof(T));
    }
    
    inline int getWidth() const { return width; }
    inline int getHeight() const { return height; }
    inline bool inside(int x, int y) const { return x>=0 && y>=0 && x<width && y<height; }
};
        

class Image : public Array2D<uint8_t>
{
public:
    Image(const Image& im) : Array2D<uint8_t>(im) {}
    Image(int w, int h) : Array2D<uint8_t>(w, h) {}
    
    inline bool isOpaque(int x, int y) const { return (*this)[y][x] != 0; }

    static Image readPGM(FILE *file);
    static Image readPGM(const char *filename);
    void writePGM(FILE* file) const;
    void writePGM(const char* filename) const;

    inline void fill(uint8_t with) { memset(data, with, width*height); }
    void drawRect(int x0, int x1, int y0, int y1, uint8_t val);
};

#endif
