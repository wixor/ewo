#include <cstdlib>
#include <cstdio>
#include <stdexcept>

#include <cairo.h>

#include "workers.h"
#include "composite.h"

CairoImage::~CairoImage() {
    if(cr_surface) {
        cairo_surface_finish(cr_surface);
        cairo_surface_destroy(cr_surface);
    }
    free(data);
}

void CairoImage::resize(int w, int h)
{
    if(width == w && height == h)
        return;

    if(cr_surface)
        cairo_surface_destroy(cr_surface);

    width = w; height = h;
    stride = cairo_format_stride_for_width(CAIRO_FORMAT_RGB24, width);

    data = realloc(data, height*stride);
    if(data == NULL)
        throw std::bad_alloc();

    cr_surface = cairo_image_surface_create_for_data((unsigned char *)data, CAIRO_FORMAT_RGB24, width, height, stride);
    if(cairo_surface_status(cr_surface) == CAIRO_STATUS_NO_MEMORY)
        throw std::bad_alloc();
}

void CairoImage::fromImage(const Image &src, bool transparency)
{
    resize(src.getWidth(), src.getHeight());
    for(int y=0; y<height; y++)
        for(int x=0; x<width; x++)
            (*this)[y][x] = (transparency && src[y][x] == 0)
                                ? (((x>>5) ^ (y>>5)) & 1) ? 0xe0e0e0 : 0xc0c0c0
                                : 0x010101*src[y][x];
}


void CairoImage::toImage(Image *dst)
{
    dst->resize(width, height);
    for(int y=0; y<height; y++)
        for(int x=0; x<width; x++)
            (*dst)[y][x] = (*this)[y][x]&0xFF;
}

/* ------------------------------------------------------------------------- */

static cairo_matrix_t makeCairoMatrix(const Matrix33 &M)
{
    cairo_matrix_t ret;
    ret.xx = M[0][0]; ret.xy = M[0][1]; ret.x0 = M[0][2];
    ret.yx = M[1][0]; ret.yy = M[1][1]; ret.y0 = M[1][2];
    return ret;
}

void Composite::prepare(const Image &src, CairoImage *dst)
{
    dst->resize(src.getWidth(), src.getHeight());
    for(int y=0; y<src.getHeight(); y++)
        for(int x=0; x<src.getWidth(); x++)
            (*dst)[y][x] = src[y][x];
}

void Composite::do_transform(const CairoImage &src, const Matrix33 &M, CairoImage *dst)
{
    cairo_t *cairo = cairo_create(dst->getCairoSurface());
    cairo_set_antialias(cairo, CAIRO_ANTIALIAS_NONE);

    cairo_matrix_t matrix = makeCairoMatrix(M.inverse());
    cairo_pattern_t *srcpatt =
        cairo_pattern_create_for_surface(src.getCairoSurface());
    cairo_pattern_set_matrix (srcpatt, &matrix);

    cairo_set_source_rgb(cairo, 1,1,1);
    cairo_paint(cairo);
    cairo_set_source(cairo, srcpatt);
    cairo_paint(cairo);

    cairo_pattern_destroy(srcpatt);
    cairo_destroy(cairo);
}

void Composite::transform(const CairoImage &src, const Matrix33 &M, CairoImage *dst)
{
    do_transform(src,M,dst);

    for(int y=0; y<dst->getHeight(); y++)
        for(int x=0; x<dst->getWidth(); x++)
            (*dst)[y][x] = ((*dst)[y][x]&0xff)*0x010101;
}

void Composite::difference(const CairoImage &known, const Image &alien, const Matrix33 &M, CairoImage *dst)
{
    dst->resize(alien.getWidth(), alien.getHeight());
    do_transform(known, M, dst);

    for(int y=0; y<dst->getHeight(); y++)
        for(int x=0; x<dst->getWidth(); x++) {
            uint32_t p = (*dst)[y][x];
            (*dst)[y][x] = (std::abs((int)alien[y][x] - (int)(p&0xff)) & ~(p>>8))*0x010101;
        }
}

float Composite::difference(const CairoImage &known, const Image &alien, const Matrix33 &M)
{
    CairoImage diff(alien.getWidth(), alien.getHeight());
    do_transform(known, M, &diff);

    float result = 0, area = 0;
    for(int y=0; y<diff.getHeight(); y++)
        for(int x=0; x<diff.getWidth(); x++) {
            uint32_t p = diff[y][x];
            int w = !(p&0x0100);
            int d = std::abs((int)alien[y][x] - (int)(p&0xff));
            result += (float)d*d*d/(256*256*256)*w;
            area += w;
        }

    return 100.f*sqrtf(result/area);
}
