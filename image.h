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
    
    bool isOpaque(int x, int y) const { return (*this)[y][x] != 0; }

    static Image readPGM(const char *filename);
    void writePGM(FILE* file = stdin) const;
    
    /* Karol's */
    int getWidth() const { return width; }
    int getHeight() const { return height; }
    void move(int dw, int dh); //very not const
    void randomInit(); //very not const
    Image &difference(const Image& im) const;
    inline uint8_t abs(uint8_t a, uint8_t b) const { return a<b ? b-a : a-b; }
    Image contour() const { Image img2(*this); img2.move(1,1); return this->difference(img2); }
    static Image contour(const char* filename) { return readPGM(filename).contour(); }
    void writePGM(const char* filename) { FILE* file = fopen(filename, "w"); writePGM(file); }
    
};

