#include <unistd.h>

#include "image.h"
#include "gui.h"

int main(int argc, char *argv[])
{
    gui_gtk_init(&argc, &argv);

    {    
        DisplaySlot ds("");
        ds.rename("moj display slot numer %d", 42);
        ds.recaption("ma adres %p", &ds);

        Image im(256,192);

        for(int i=0; i<256; i++)
        {
            for(int y=0;y<192;y++)
                for(int x=0;x<256;x++)
                    im[y][x] = (((y>>5)^(x>>5))&7) == (i&7) ? 0x01 : 0x00;
            ds.update(im);
            usleep(250000);
        }
    }

    pause();

    return 0;
}
