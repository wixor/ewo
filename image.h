#ifndef __IMAGE_H__
#define __IMAGE_H__

#include <stdint.h>
#include <cstdlib>
#include <cstring>
#include <cassert>
#include <stdexcept>

#include <cairo.h>
#include <cairo-xlib.h>

#include "util.h"

/* stuff in image.c */
extern "C" {
    int img_read(const char *filename, int *widthp, int *heightp, uint8_t **bytesp);
    int img_write(const char *filename, int width, int height, const uint8_t *bytes);
    uint32_t img_checksum(int width, int height, const uint8_t *bytes);
};

/* smart-ass wrapper for cairo_surface_t employing cairo's reference counting.
 * do not use pointers or references to CairoImage, use only values
 * (that means no CairoImage* or CairoImage&, just pure CairoImage).
 * 
 * CairoImages are created by uploading image data to the X server.
 * you can obtain CairoImage from Image using gui_upload (see gui.h). */
class CairoImage
{
    cairo_surface_t *surface;

public:
    inline CairoImage() : surface(NULL) { }
    inline CairoImage(cairo_surface_t *surface) : surface(surface) { }
    inline CairoImage(const CairoImage &ci) : surface(ci.surface) { cairo_surface_reference(surface); }
    inline ~CairoImage() { if(surface) cairo_surface_destroy(surface); }

    inline CairoImage& operator=(const CairoImage &ci)
    {
        if(surface) cairo_surface_destroy(surface);
        surface = ci.surface;
        cairo_surface_reference(surface);
        return *this;
    }
    
    operator cairo_surface_t* () const { return surface; }

    inline int getWidth() {
        switch(cairo_surface_get_type(surface)) {
            case CAIRO_SURFACE_TYPE_IMAGE: return cairo_image_surface_get_width(surface);
            case CAIRO_SURFACE_TYPE_XLIB:  return cairo_xlib_surface_get_width(surface);
            default: assert("unknown surface type"); abort();
        }
    }

    inline int getHeight() {
        switch(cairo_surface_get_type(surface)) {
            case CAIRO_SURFACE_TYPE_IMAGE: return cairo_image_surface_get_height(surface);
            case CAIRO_SURFACE_TYPE_XLIB:  return cairo_xlib_surface_get_height(surface);
            default: assert("unknown surface type"); abort();
        }
    }
};

/* our images
 * you can load and save to pgm's, png's and jpeg's.
 * however bear in mind that when loading colour images, only the red channel is used! */
class Image : public Array2D<uint8_t>
{
public:
    inline Image() : Array2D<uint8_t>() { }
    inline Image(int w, int h) : Array2D<uint8_t>(w, h) {}
    inline Image(const Image& im) : Array2D<uint8_t>(im) {}
        
<<<<<<< HEAD
    static Image readPGM(FILE *file);
    static Image readPGM(const char *filename);
    void writePGM(FILE* file) const;
    void writePGM(const char* filename) const;

    inline void fill(uint8_t with) { memset(data, with, width*height); }
    void drawRect(int x0, int x1, int y0, int y1, uint8_t val);

    uint32_t checksum() const;
};

/* ------------------------------------------------------------------------ */

struct POI {
    int x, y;
    float val;

    inline POI() { }
    inline POI(int x, int y, float v) : x(x), y(y), val(v) { }
    inline bool operator<(const POI &p) const {
        return val != p.val ? val > p.val :
                 x != p.x   ? x   < p.x :
                              y   < p.y;
    }
    static inline int dist2(const POI &a, const POI &b) { 
        return (a.x-b.x)*(a.x-b.x) + (a.y-b.y)*(a.y-b.y);
    static inline float dist(const POI &a, const POI &b) {
        return sqrtf(dist2(a,b));
    }
};

/* ------------------------------------------------------------------------ */

class Matrix33
{
    float data[9];

public:
    inline float* operator[](int row) { return data+row*3; }
    inline const float* operator[](int row) const { return data+row*3; }

    inline Matrix33()
    {
        Matrix33 &me = *this;
        me[0][0] = me[1][1] = me[2][2] = 1;
        me[0][1] = me[0][2] = me[1][0] = me[1][2] = me[2][0] = me[2][1] = 0;
    }

    static inline Matrix33 translation(float dx, float dy)
    {
        Matrix33 ret;
        ret[0][0] = ret[1][1] = ret[2][2] = 1.0f;
        ret[0][2] = dx; ret[1][2] = dy;
        return ret;
    }
    static inline Matrix33 rotation(float alpha)
    {
        Matrix33 ret;
        ret[0][0] = ret[1][1] = cosf(alpha);
        ret[1][0] = -(ret[0][1] = sinf(alpha));
        ret[2][2] = 1.0f;
        return ret;
    }
    static inline Matrix33 scaling(float sx, float sy)
    {
        Matrix33 ret;
        ret[0][0] = sx; ret[1][1] = sy;
        ret[2][2] = 1.0f;
        return ret;
    }

    inline Matrix33 operator+(const Matrix33 &M) const
=======
    inline static Image read(const char *filename)
>>>>>>> 81f22aaf48f8863ad93630b916ebad3984e7abda
    {
        Image ret;
        if(img_read(filename, &ret.width,&ret.height,&ret.data) == -1)
            throw std::runtime_error("failed to read image");
        return ret;
    }

    inline void write(const char* filename) const
    {
        if(img_write(filename, width,height,data) == -1)
            throw std::runtime_error("failed to write image");
    }

    inline void fill(uint8_t with) {
        memset(data, with, width*height);
    }

    inline uint32_t checksum() const {
        return img_checksum(width,height,data);
    }
};

#endif
