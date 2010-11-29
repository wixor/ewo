#include "image.h"

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

Image &operator=(const Image &im)
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

Image Image::readPGM(const char *filename)
{
}

void Image::writePGM(void) const
{
}

