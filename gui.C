#include <cstdio>
#include <cstring>
#include <cassert>
#include <stdarg.h>
#include <vector>

#include <cairo.h>
#include <cairo-xlib.h>

#include "image.h"
#include "gui.h"

DisplaySlot::DisplaySlot(const char *name)
{
    pthread_mutex_init(&mutex, NULL);
    width = height = 0;
    this->name = name;
    active = false;
}

void DisplaySlot::activate()
{
    if(active) return;
    gui_register(this);
    active = true;
}

void DisplaySlot::deactivate()
{
    if(!active) return;
    gui_unregister(this);
    active = false;
}

DisplaySlot::~DisplaySlot()
{
    assert(!active);
    pthread_mutex_destroy(&mutex);
}

extern "C" void displayslot_paint(struct displayslot *ds_)
{
    DisplaySlot *ds = (DisplaySlot *)ds_;
    ds->lock();
    ds->draw();
    ds->unlock();
}

void DisplaySlot::fill(rgba color)
{
    cairo_set_source_rgba(cr, color.r,color.g,color.b,color.a);
    cairo_paint(cr);
}

void DisplaySlot::drawImage(CairoImage ci, const Matrix *m, bool difference)
{
    cairo_surface_t *surface = ci;
    cairo_pattern_t *pattern = cairo_pattern_create_for_surface(surface);

    cairo_matrix_t m1, m2;
    if(m) {
        cairo_matrix_init(&m1,
            (*m)[0][0], (*m)[1][0],
            (*m)[0][1], (*m)[1][1],
            (*m)[0][2], (*m)[1][2]);
        cairo_get_matrix(cr, &m2);
        cairo_matrix_multiply(&m1, &m1, &m2);
        cairo_set_matrix(cr, &m1);
    }

    int w = ci.getWidth(), h = ci.getHeight();

    cairo_set_source(cr, pattern);
    if(difference)
        cairo_paint_with_alpha(cr, 0.5);
    else {
        cairo_rectangle(cr, 0,0, w,h);
        cairo_fill(cr);
    }

    if(m)
        cairo_set_matrix(cr, &m2);

    cairo_pattern_destroy(pattern);
}

void DisplaySlot::drawDot(Point p, float d, rgba color)
{
    cairo_set_source_rgba(cr, color.r,color.g,color.b,color.a);
    cairo_rectangle(cr, p.x-.5f*d, p.y-.5f*d, d,d);
    cairo_fill(cr);
}

void DisplaySlot::drawDots(std::vector<Point> ps, float d, rgba color)
{
    cairo_set_source_rgba(cr, color.r,color.g,color.b,color.a);
    for(int i=0; i<(int)ps.size(); i++)
        cairo_rectangle(cr, ps[i].x-.5f*d, ps[i].y-.5f*d, d,d);
    cairo_fill(cr);
}

