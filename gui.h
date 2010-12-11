#ifndef __GUI_H__
#define __GUI_H__

#include "image.h"
#include "gui-gtk.h"
#include <stdarg.h>

class DisplaySlot
{
    struct displayslot ds;

public:
    inline DisplaySlot() { displayslot_init(&ds); }
    inline DisplaySlot(const char *name) { displayslot_init(&ds); rename("%s", name); }
    inline ~DisplaySlot() { displayslot_cleanup(&ds); }
    

    inline void rename(const char *fmt, ...) {
        va_list args; va_start(args, fmt);
        char *buf; vasprintf(&buf, fmt, args);
        va_end(args);
        
        displayslot_rename(&ds, buf);
    }

    inline void recaption(const char *fmt, ...) {
        va_list args; va_start(args, fmt);
        char *buf; vasprintf(&buf, fmt, args);
        va_end(args);

        displayslot_recaption(&ds, buf);
    }

    inline void update(const Image &im) { displayslot_update(&ds, im.getWidth(), im.getHeight(), im[0]); }
};

#endif

