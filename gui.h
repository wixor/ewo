#ifndef __GUI_H__
#define __GUI_H__

#include "image.h"
#include "gui-gtk.h"

class DisplaySlot : private displayslot
{
    class CairoImage img;

    inline void sendEvent(int ev);
    inline void sendEventSync(int ev);

public:
    DisplaySlot(const char *initname);
    ~DisplaySlot();

    void rename(const char *fmt, ...);
    void recaption(const char *fmt, ...);
    void update(const Image &src);
    void bind();
    void unbind();
};

#endif

