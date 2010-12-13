#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <cairo.h>

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

void Image::drawRect(int x0, int x1, int y0, int y1, uint8_t val)
{
    for(int y=y0; y<=y1; y++)
        for(int x=x0; x<=x1; x++)
            if(inside(x,y))
                (*this)[y][x] = val;
}

uint32_t Image::checksum() const
{
    uint32_t sum1 = 0xffff, sum2 = 0xffff;

    int len = width*height/2;
    const uint16_t *ptr = (const uint16_t *)data;

    while (len) {
        int block = std::min(360, len);
        len -= block;
        while(block--) 
            sum1 += *ptr++,
            sum2 += sum1;
        sum1 = (sum1 & 0xffff) + (sum1 >> 16);
        sum2 = (sum2 & 0xffff) + (sum2 >> 16);
    }
    
    if(width*height%2 == 1)
        sum1 += data[width*height-1], sum2 += sum1;
    
    sum1 = (sum1 & 0xffff) + (sum1 >> 16);
    sum2 = (sum2 & 0xffff) + (sum2 >> 16);
    sum1 = (sum1 & 0xffff) + (sum1 >> 16);
    sum2 = (sum2 & 0xffff) + (sum2 >> 16);
    
    return sum2 << 16 | sum1;
}

/* ------------------------------------------------------------------------- */

CairoImage::~CairoImage() {
    if(cr_surface)
        cairo_surface_destroy((cairo_surface_t *)cr_surface);
}

void CairoImage::resize(int w, int h)
{
    if(width == w && height == h)
        return;

    if(cr_surface)
        cairo_surface_destroy((cairo_surface_t *)cr_surface);

    width = w; height = h;
    stride = cairo_format_stride_for_width(CAIRO_FORMAT_RGB24, width);

    data = realloc(data, height*stride);
    if(data == NULL)
        throw std::bad_alloc();

    cr_surface = cairo_image_surface_create_for_data((unsigned char *)data, CAIRO_FORMAT_RGB24, width, height, stride);
    if(cairo_surface_status((cairo_surface_t *)cr_surface) == CAIRO_STATUS_NO_MEMORY)
        throw std::bad_alloc();
}

void CairoImage::setFromImage(const Image &im, bool transparency)
{
    resize(im.getWidth(), im.getHeight());
    for(int y=0; y<height; y++)
        for(int x=0; x<width; x++)
            (*this)[y][x] = (transparency && im[y][x] == 0)
                                ? (((x>>5) ^ (y>>5)) & 1) ? 0xe0e0e0 : 0xc0c0c0
                                : 0x010101*im[y][x];
}

