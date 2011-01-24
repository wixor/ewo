#ifndef __GUI_H__
#define __GUI_H__

#include <pthread.h>
#include <vector>
#include <stdexcept>

#include <cairo.h>

#include "image.h"
#include "gui-gtk.h"

/* wrapper around gui_do_upload from gui-gtk.c */
static inline CairoImage gui_upload(const Image &im)
{
    cairo_surface_t *s = gui_do_upload(im.getWidth(), im.getHeight(), im[0]);
    if(!s) throw std::bad_alloc();
    return CairoImage(s);
}


struct rgba {
    float r,g,b,a;
    inline rgba() { }
    inline rgba(const rgba &c) : r(c.r), g(c.g), b(c.b), a(c.a) { }
    inline rgba(float r, float g, float b, float a = 1.f) : r(r), g(g), b(b), a(a) { }
};


class DisplaySlot : private displayslot
{
    bool active;

    void drawImage(CairoImage ci, const Matrix *m, bool difference);

protected:
    inline void resize(int w, int h) {
        width = w; height = h;
    }

    void fill(rgba color);
    inline void drawImage(CairoImage ci) { drawImage(ci, NULL, false); }
    inline void drawImage(CairoImage ci, const Matrix &m) { drawImage(ci, &m, false); }
    inline void drawDifference(CairoImage ci, const Matrix &m) { drawImage(ci, &m, true); }
    void drawDot(Point p, float d, rgba color);
    void drawDots(std::vector<Point> ps, float d, rgba color);


public:
    DisplaySlot(const char *name);
    virtual ~DisplaySlot();
    
    void activate();
    void deactivate();

    virtual void draw() = 0;

    inline void bind() {
        activate();
        gui_bind(this);
    }
    inline void unbind() {
        activate();
        gui_unbind(this);
    }

    inline void lock() {
        pthread_mutex_lock(&mutex);
    }
    inline void unlock() {
        pthread_mutex_unlock(&mutex);
    }
};

#endif

