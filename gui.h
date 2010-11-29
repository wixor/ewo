#ifndef __GUI_H__
#define __GUI_H__

#include "image.h"
#include "gui-gtk.h"
#include <stdarg.h>

class DisplaySlot
{
    struct displayslot ds;

public:
    inline DisplySlot() { displayslot_init(&ds); }
    inline ~DisplaySlot(); { displayslot_cleanup(&ds); }

    inline void rename(const char *fmt, ...) {
        va_list args; va_start(args, fmt);
        displayslot_rename(&ds, vasprintf(fmt, args));
        va_end(args);
    }

    inline void recaption(const char *fmt, ...) {
        va_list args; va_start(args, fmt);
        displayslot_recaption(&ds, vasprintf(fmt, args));
        va_end(args);
    }

    inline void update(const Image &im) { displayslot_update(im.getWidth(), im.getHeight(), im[0]); }
};

#endif

