#ifndef __IMAGE_H__
#define __IMAGE_H__

#include <stdint.h>
#include <cstring>
#include <stdexcept>

#include "util.h"

extern "C" {
int img_read(const char *filename, int *widthp, int *heightp, uint8_t **bytesp);
int img_write(const char *filename, int width, int height, const uint8_t *bytes);
uint32_t img_checksum(int width, int height, const uint8_t *bytes);
};

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

#endif
