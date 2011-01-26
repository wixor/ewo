#include <cstdlib>
#include <cstdio>

#include "image.h"

extern "C" {
    void g_type_init();
};

/* test:
 * ./image-example geom/square.pgm foo.png
 * ./image-example foo.png bar.png
 * ./image-example bar.png bar.jpg
 * ./image-example bar.jpg foo.jpg
 * ./image-example foo.jpg bar.jpg
 * ...
 */

int main(int argc, char *argv[1])
{
    g_type_init(); /* <-- potrzebne jesli nie odpalamy calego gui */
    Image im = Image::read(argv[1]);
    printf("read %s, checksum: %08x\n", argv[1], im.checksum());
    im.write(argv[2]);
    printf("written %s\n", argv[2]);

    return 0;
}

