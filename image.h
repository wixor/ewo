#ifndef __IMAGE_H__
#define __IMAGE_H__

#include <stdint.h>
#include <cstring>
#include <cstdio>


class Image
{
    int width, height;
    uint8_t *data;

public:
    Image(int w, int h);
    Image(const Image &im);
    ~Image();

    void operator=(const Image &im);
    uint8_t* operator[](int row) { return data + row*width; }
    const uint8_t *operator[](int row) const { return data + row*width; }
    
    int getWidth() const { return width; }
    int getHeight() const { return height; }

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
