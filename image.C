#include <cstdlib>
#include <cstdio>
#include <cstring>

#include <algorithm>
#include <stdexcept>

#include "image.h"

Image Image::readPGM(FILE *file)
{
    int width, height;
    if(fscanf(file, "P5 %d %d 255%*c", &width, &height) != 2)
        throw std::runtime_error("malformed pgm");

    Image ret(width, height);
    if(fread(ret.data, width*height, 1, file) != 1) 
        throw std::runtime_error("malformed pgm");

    return ret;
}

Image Image::readPGM(const char *filename)
{
    FILE *file = fopen(filename, "rb");
    
    if(file == NULL)
        throw std::runtime_error("failed to open file");

    try {
        Image im = readPGM(file);
        fclose(file);
        return im;
    } catch(const std::exception &e) {
        fclose(file);
        throw e;
    }
}

void Image::writePGM(FILE* file) const
{
    fprintf(file, "P5\n%d %d\n255\n", width, height);
    fwrite(data, width*height, 1, file);
}
void Image::writePGM(const char *filename) const
{
    FILE *f = fopen(filename, "wb");
    if(f == NULL)
        throw std::runtime_error("failed to open file");
    writePGM(f);
    fclose(f);
}

#if 0

void Image::move(int dw, int dh)
{
    Image tmp(width, height);

    for (int i=0; i<height; i++)
        for (int j=0; j<width; j++)
            tmp[i][j] = 
                i-dh >= 0 && j-dw >=0 && i-dh < height && j-dw < width
                    ? (*this)[i-dh][j-dw]
                    : 0;

    (*this) = tmp;
}

void Image::randomInit()
{
    for (int i=0; i<height; i++)
        for (int j=0; j<width; j++)
            (*this)[i][j] = rand()&255;
}

Image Image::difference(const Image& im) const //wartość bezwzględna różnicy między dwoma obrazkami, pixel po pixelu
{
    Image ret(width, height);
    for (int i=0; i<height; i++) 
        for (int j=0; j<width; j++)
            ret[i][j] = std::abs((int)(*this)[i][j] - (int)im[i][j]);
    return ret;
}

#endif
