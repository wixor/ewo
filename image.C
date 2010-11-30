#include "image.h"
#include <cstring>
#include <cstdlib>
#include <cstdio>

Image::Image(int w, int h)
{
    width = w;
    height = h;
    data = new uint8_t[width*height];
}

Image::Image(const Image &im)
{
    width = im.width;
    height = im.height;
    data = new uint8_t[width*height];
    memcpy(data, im.data, width*height);
}

Image::~Image() {
    delete data;
}

void Image::operator=(const Image &im)
{
    if(width != im.width || height != im.height)
    {
        width = im.width;
        height = im.height;

        delete data;
        data = new uint8_t[width*height];
    }
    memcpy(data, im.data, width*height);
}

Image Image::readPGM(const char *filename) //Karol's
{
    FILE* file = fopen(filename, "r");
    if (file == NULL) printf("unable to open %s\n", filename);
    char* buf = new char[1024]; int bufsize = 1024;
    char tmp[100];
    int width, height;
    for (int i=0;;)
    {
        getline(&buf, (size_t*)&bufsize, file);
        if (buf[0] == '#') continue;
        if (i == 0) ;
        else if (i == 1) sscanf(buf, "%d %d", &width, &height);
        else if (i == 2) break;
        i ++;
    }
    
    // fprintf(stderr, "%d %d\n", width, height);
    Image* ret = new Image(width, height);
    for (int i=0; i<height; i++)
        for  (int j=0; j<width; j++)
            fscanf(file, "%c", &(*ret)[i][j]);
    
    return *ret;
}

void Image::writePGM(FILE* file) const
{
    fprintf(file, "P5\n%d %d\n255\n", width, height);
    for (int i=0; i<height; i++)
        for  (int j=0; j<width; j++)
            fprintf(file, "%c", data[i*width+j]);
}

/* Karol's part */

void Image::move(int dw, int dh)
{
    uint8_t* newdata = new uint8_t[width*height];
    for (int i=0; i<height; i++)
        for (int j=0; j<width; j++)
        {
            int ind = width*(i-dh) + j-dw;
            newdata[i*width+j] = i-dh >= 0 && j-dw >=0 && i-dh < height && j-dw < width ? data[ind] : 0;
        }
    memcpy(data, newdata, width*height);
}

void Image::randomInit()
{
    for (int i=0; i<height; i++)
        for (int j=0; j<width; j++)
            data[i*width+j] = rand()&255;
}

Image &Image::difference(const Image& im) const //wartość bezwzględna różnicy między dwoma obrazkami, pixel po pixelu
{
    Image* ret = new Image(*this);
    for (int i=0; i<height; i++) 
        for (int j=0; j<width; j++)
            (*ret)[i][j] = (uint8_t)abs((*ret)[i][j] , im[i][j]);
    return *ret;
}
    
