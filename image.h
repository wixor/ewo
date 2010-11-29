#include <stdint.h>

class Image
{
    int width, height;
    uint8_t *data;

public:
    Image(int w, int h);
    Image(const Image &im);
    ~Image();

    Image &operator=(const Image &im);
    uint8_t* operator[](int row) { return data + idx*w; }
    const uint8_t *operator[](int row) const { return data + idx*w; }

    bool isOpaque(int x, int y) const { return (*this)[y][x] != 0; }

    static Image readPGM(const char *filename);
    void writePGM(void) const;
};

