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
        if(surface) cairo_surface_reference(surface);
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
        
    inline static Image read(const char *filename)
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

class ColorImage : public Array2D<uint32_t>
{
public:
    inline ColorImage() : Array2D<uint32_t>() { }
    inline ColorImage(int w, int h) : Array2D<uint32_t>(w, h) {}
    inline ColorImage(const ColorImage& im) : Array2D<uint32_t>(im) {}
};

#endif
