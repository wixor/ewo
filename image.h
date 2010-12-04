#ifndef __IMAGE_H__
#define __IMAGE_H__

#include <stdint.h>
#include <cstring>
#include <cstdio>

template <typename T> class ImageBase
{
protected:
    int width, height;
    T *data;
public:
    inline ImageBase(int w, int h)
    {
        width = w;
        height = h;
        data = new T[width*height];
    }
    inline ImageBase(const ImageBase<T> &im)
    {
        width = im.width;
        height = im.height;
        data = new T[width*height];
        memcpy(data, im.data, width*height*sizeof(T));
    }
    virtual inline ~ImageBase() { delete data; }
    
    inline T* operator[](int row) { return data+row*width; }
    inline const T* operator[](int row) const { return data+row*width; }
    inline void operator=(const ImageBase<T>& im)
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
};
        

class Image : public ImageBase<uint8_t>
{
public:
    
    Image(const Image& im) : ImageBase<uint8_t>(im) {}
    Image(int w, int h) : ImageBase<uint8_t>(w, h) {}
    
    inline bool isOpaque(int x, int y) const { return (*this)[y][x] != 0; }

    static Image readPGM(FILE *file);
    static Image readPGM(const char *filename);
    void writePGM(FILE* file) const;
    void writePGM(const char* filename) const;

    inline void fill(uint8_t with) { memset(data, with, width*height); }
    
    void move(int dw, int dh); //very not const
    void randomInit(); //very not const
    Image difference(const Image& im) const;
    Image contour() const { Image img2(*this); img2.move(1,1); return this->difference(img2); }
    static Image contour(const char* filename) { return readPGM(filename).contour(); }
};

#endif
