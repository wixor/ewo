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
    ImageBase(int w, int h);
    ImageBase(const ImageBase<T> &im);
    virtual ~ImageBase() { delete data; }
    
    T* operator[](int row) { return data+row*width; }
    const T* operator[](int row) const { return data+row*width; }
    void operator=(const ImageBase<T>& im);
    
    int getWidth() const { return width; }
    int getHeight() const { return height; }
};
        

class Image : public ImageBase<uint8_t>
{
public:
    
    Image(const Image& im) : ImageBase<uint8_t>(im) {}
    Image(int w, int h) : ImageBase<uint8_t>(w, h) {}
    
    bool isOpaque(int x, int y) const { return (*this)[y][x] != 0; }

    static Image readPGM(FILE *file);
    static Image readPGM(const char *filename);
    void writePGM(FILE* file) const;
    void writePGM(const char* filename) const;

    void fill(uint8_t with);
    
    void move(int dw, int dh); //very not const
    void randomInit(); //very not const
    Image difference(const Image& im) const;
    Image contour() const { Image img2(*this); img2.move(1,1); return this->difference(img2); }
    static Image contour(const char* filename) { return readPGM(filename).contour(); }
};

#endif
